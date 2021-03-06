/*
 * KiRouter - a push-and-(sometimes-)shove PCB router
 *
 * Copyright (C) 2013-2015 CERN
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <boost/foreach.hpp>
#include <boost/optional.hpp>

#include "trace.h"

#include "pns_node.h"
#include "pns_walkaround.h"
#include "pns_shove.h"
#include "pns_utils.h"
#include "pns_diff_pair_placer.h"
#include "pns_solid.h"
#include "pns_topology.h"

using boost::optional;

PNS_DIFF_PAIR_PLACER::PNS_DIFF_PAIR_PLACER() :
    PNS_PLACEMENT_ALGO()
{
    m_state = RT_START;
    m_chainedPlacement = false;
    m_initialDiagonal = false;
    m_startDiagonal = false;
    m_fitOk = false;
    m_netP = 0;
    m_netN = 0;
    m_iteration = 0;
    m_world = NULL;
    m_shove = NULL;
    m_currentNode = NULL;
    m_lastNode = NULL;
    m_placingVia = false;
    m_viaDiameter = 0;
    m_viaDrill = 0;
    m_currentWidth = 0;
    m_currentNet = 0;
    m_currentLayer = 0;
    m_startsOnVia = false;
    m_orthoMode = false;
    m_snapOnTarget = false;
    m_currentEndItem = NULL;
    m_currentMode = RM_MarkObstacles;
    m_idle = true;
}

PNS_DIFF_PAIR_PLACER::~PNS_DIFF_PAIR_PLACER()
{
    if( m_shove )
        delete m_shove;
}

const PNS_VIA PNS_DIFF_PAIR_PLACER::makeVia( const VECTOR2I& aP, int aNet )
{
    const PNS_LAYERSET layers( SizeSettings().GetLayerTop(), SizeSettings().GetLayerBottom() );

    PNS_VIA v( aP, layers, SizeSettings().ViaDiameter(), SizeSettings().ViaDrill(), -1, SizeSettings().ViaType() );
    v.SetNet( aNet );

    return v;
}


void PNS_DIFF_PAIR_PLACER::SetOrthoMode ( bool aOrthoMode )
{
    m_orthoMode = aOrthoMode;

    if( !m_idle )
        Move( m_currentEnd, NULL );
}


bool PNS_DIFF_PAIR_PLACER::ToggleVia( bool aEnabled )
{
    m_placingVia = aEnabled;

    if( !m_idle )
        Move( m_currentEnd, NULL );

    return true;
}


bool PNS_DIFF_PAIR_PLACER::rhMarkObstacles( const VECTOR2I& aP )
{
    if( !routeHead( aP ) )
        return false;

    bool collP = static_cast<bool>( m_currentNode->CheckColliding( &m_currentTrace.PLine() ) );
    bool collN = static_cast<bool>( m_currentNode->CheckColliding( &m_currentTrace.NLine() ) );

    m_fitOk = !( collP || collN ) ;

    return m_fitOk;
}


bool PNS_DIFF_PAIR_PLACER::propagateDpHeadForces ( const VECTOR2I& aP, VECTOR2I& aNewP )
{
    PNS_VIA virtHead = makeVia( aP, -1 );

    if( m_placingVia )
        virtHead.SetDiameter( viaGap() + 2 * virtHead.Diameter() );
    else
    {
        virtHead.SetLayer( m_currentLayer );
        virtHead.SetDiameter( SizeSettings().DiffPairGap() + 2 * SizeSettings().TrackWidth() );
    }

    VECTOR2I lead( 0, 0 );// = aP - m_currentStart ;
    VECTOR2I force;
    bool solidsOnly = true;

    if( m_currentMode == RM_MarkObstacles )
    {
        aNewP = aP;
        return true;
    }
    else if( m_currentMode == RM_Walkaround )
    {
        solidsOnly = false;
    }

    // fixme: I'm too lazy to do it well. Circular approximaton will do for the moment.
    if( virtHead.PushoutForce( m_currentNode, lead, force, solidsOnly, 40 ) )
    {
        aNewP = aP + force;
        return true;
    }

    return false;
}


bool PNS_DIFF_PAIR_PLACER::attemptWalk ( PNS_NODE* aNode, PNS_DIFF_PAIR* aCurrent, PNS_DIFF_PAIR& aWalk, bool aPFirst, bool aWindCw, bool aSolidsOnly )
{
    PNS_WALKAROUND walkaround( aNode );
    PNS_WALKAROUND::WALKAROUND_STATUS wf1;

    aNode->TemporallyOverrideClearance( true, aCurrent->NetP(), aCurrent->NetN(), aCurrent->Gap() );

    walkaround.SetSolidsOnly( aSolidsOnly );
    walkaround.SetIterationLimit( RoutingSettings().WalkaroundIterationLimit() );

    PNS_SHOVE shove( aNode );
    PNS_LINE walkP, walkN;

    aWalk = *aCurrent;

    int iter = 0;

    PNS_DIFF_PAIR cur( *aCurrent );

    bool currentIsP = aPFirst;

    int mask = aSolidsOnly ? PNS_ITEM::SOLID : PNS_ITEM::ANY;

    do
    {
        PNS_LINE preWalk = ( currentIsP ? cur.PLine() : cur.NLine() );
        PNS_LINE preShove = ( currentIsP ? cur.NLine() : cur.PLine() );
        PNS_LINE postWalk;

        if( !aNode->CheckColliding ( &preWalk, mask ) )
        {
            currentIsP = !currentIsP;

            if( !aNode->CheckColliding( &preShove, mask ) )
                break;
            else
                continue;
        }

        wf1 = walkaround.Route( preWalk, postWalk, false );

        if( wf1 != PNS_WALKAROUND::DONE )
            return false;

        PNS_LINE postShove( preShove );

        shove.ForceClearance( true, cur.Gap() - 2 * PNS_HULL_MARGIN );

        PNS_SHOVE::SHOVE_STATUS sh1;

        sh1 = shove.ProcessSingleLine( postWalk, preShove, postShove );

        if( sh1 != PNS_SHOVE::SH_OK )
            return false;

        postWalk.Line().Simplify();
        postShove.Line().Simplify();

        cur.SetShape( postWalk.CLine(), postShove.CLine(), !currentIsP );

        currentIsP = !currentIsP;

        if( !aNode->CheckColliding( &postShove, mask ) )
            break;

        iter++;
    }
    while( iter < 3 );

    if( iter == 3 )
        return false;

    aWalk.SetShape( cur.CP(), cur.CN() );

    aNode->TemporallyOverrideClearance( false );

    return true;
}


bool PNS_DIFF_PAIR_PLACER::tryWalkDp( PNS_NODE* aNode, PNS_DIFF_PAIR &aPair, bool aSolidsOnly )
{
    PNS_DIFF_PAIR best;
    double bestScore = 100000000000000.0;

    for( int attempt = 0; attempt <= 1; attempt++ )
    {
        PNS_DIFF_PAIR p;
        PNS_NODE *tmp = m_currentNode->Branch();

        bool pfirst = attempt % 2 ? true : false;
        bool wind_cw = attempt / 2 ? true : false;

        if( attemptWalk ( tmp, &aPair, p, pfirst, wind_cw, aSolidsOnly ) )
        {
        //    double len = p.TotalLength();
            double cl = p.CoupledLength();
            double skew = p.Skew();

            double score = cl + fabs(skew) * 3.0;

            if( score < bestScore )
            {
                bestScore = score;
                best = p;
            }
        }

        delete tmp;
    }

    if( bestScore > 0.0 )
    {
        PNS_OPTIMIZER optimizer( m_currentNode );

        aPair.SetShape( best );
        optimizer.Optimize( &aPair );

        return true;
    }

    return false;
}


bool PNS_DIFF_PAIR_PLACER::rhWalkOnly( const VECTOR2I& aP )
{
    if( !routeHead ( aP ) )
        return false;

    m_fitOk = tryWalkDp( m_currentNode, m_currentTrace, false );

    return m_fitOk;
}


bool PNS_DIFF_PAIR_PLACER::route( const VECTOR2I& aP )
{
    switch( m_currentMode )
    {
    case RM_MarkObstacles:
        return rhMarkObstacles( aP );
    case RM_Walkaround:
        return rhWalkOnly( aP );
    case RM_Shove:
        return rhShoveOnly( aP );
    default:
        break;
    }

    return false;
}


bool PNS_DIFF_PAIR_PLACER::rhShoveOnly( const VECTOR2I& aP )
{
    m_currentNode = m_shove->CurrentNode();

    bool ok = routeHead( aP );

    m_fitOk  = false;

    if( !ok )
        return false;

    if( !tryWalkDp( m_currentNode, m_currentTrace, true ) )
        return false;

    PNS_LINE pLine( m_currentTrace.PLine() );
    PNS_LINE nLine( m_currentTrace.NLine() );
    PNS_ITEMSET head;

    head.Add( &pLine );
    head.Add( &nLine );

    PNS_SHOVE::SHOVE_STATUS status = m_shove->ShoveMultiLines( head );

    m_currentNode = m_shove->CurrentNode();

    if( status == PNS_SHOVE::SH_OK )
    {
        m_currentNode = m_shove->CurrentNode();

        if( !m_currentNode->CheckColliding( &m_currentTrace.PLine() ) &&
            !m_currentNode->CheckColliding( &m_currentTrace.NLine() ) )
        {
            m_fitOk = true;
        }
    }

    return m_fitOk;
}


const PNS_ITEMSET PNS_DIFF_PAIR_PLACER::Traces()
{
      PNS_ITEMSET t;

      t.Add( const_cast<PNS_LINE*>( &m_currentTrace.PLine() ) );
      t.Add( const_cast<PNS_LINE*>( &m_currentTrace.NLine() ) );

      return t;
}


void PNS_DIFF_PAIR_PLACER::FlipPosture()
{
    m_startDiagonal = !m_startDiagonal;

    if( !m_idle )
        Move( m_currentEnd, NULL );
}


PNS_NODE* PNS_DIFF_PAIR_PLACER::CurrentNode( bool aLoopsRemoved ) const
{
    if( m_lastNode )
        return m_lastNode;

    return m_currentNode;
}


bool PNS_DIFF_PAIR_PLACER::SetLayer( int aLayer )
{
    if( m_idle )
    {
        m_currentLayer = aLayer;
        return true;
    } else if( m_chainedPlacement )
        return false;
    else if( !m_prevPair )
        return false;
    else if( m_prevPair->PrimP() || ( m_prevPair->PrimP()->OfKind( PNS_ITEM::VIA ) &&
                m_prevPair->PrimP()->Layers().Overlaps( aLayer ) ) )
    {
        m_currentLayer = aLayer;
        m_start = *m_prevPair;
        initPlacement();
        Move( m_currentEnd, NULL );
        return true;
    }

    return false;
}


OPT_VECTOR2I PNS_DIFF_PAIR_PLACER::getDanglingAnchor( PNS_NODE* aNode, PNS_ITEM* aItem )
{
    switch( aItem->Kind() )
    {
    case PNS_ITEM::VIA:
    case PNS_ITEM::SOLID:
        return aItem->Anchor( 0 );

    case PNS_ITEM::SEGMENT:
    {
        PNS_SEGMENT* s =static_cast<PNS_SEGMENT*>( aItem );

        PNS_JOINT* jA = aNode->FindJoint( s->Seg().A, s );
        PNS_JOINT* jB = aNode->FindJoint( s->Seg().B, s );

        if( jA->LinkCount() == 1 )
            return s->Seg().A;
        else if( jB->LinkCount() == 1 )
            return s->Seg().B;
        else
            return OPT_VECTOR2I();
    }

    default:
        return OPT_VECTOR2I();
        break;
    }
}


bool PNS_DIFF_PAIR_PLACER::findDpPrimitivePair( const VECTOR2I& aP, PNS_ITEM* aRefItem, PNS_DP_PRIMITIVE_PAIR& aPair )
{
    if (m_world->IsPairedNet( aRefItem->Net() ))
        return false;

    int refNet = aRefItem->Net();
    int pairedNet = m_world->PairedNet( refNet );

    std::set<PNS_ITEM*> items;

    OPT_VECTOR2I refAnchor = getDanglingAnchor( m_currentNode, aRefItem );

    if( !refAnchor )
        return false;

    m_currentNode->AllItemsInNet( pairedNet, items );
    double bestDist = std::numeric_limits<double>::max();
    bool found = false;

    BOOST_FOREACH( PNS_ITEM* item, items )
    {
        if( item->Kind() == aRefItem->Kind() )
        {
            OPT_VECTOR2I anchor = getDanglingAnchor( m_currentNode, item );
            if( !anchor )
                continue;

            double dist = ( *anchor - *refAnchor ).EuclideanNorm();

            bool shapeMatches = true;

            if( item->OfKind( PNS_ITEM::SOLID ) && item->Layers() != aRefItem->Layers() )
            {
                shapeMatches = false;
            }

            if( dist < bestDist && shapeMatches )
            {
                found = true;
                bestDist = dist;

                if( m_world->PairingPolarity( refNet ) > 0 )
                {
                    aPair = PNS_DP_PRIMITIVE_PAIR( aRefItem, item );
                    aPair.SetAnchors( *refAnchor, *anchor );
                }
                else
                {
                    aPair = PNS_DP_PRIMITIVE_PAIR ( item, aRefItem );
                    aPair.SetAnchors( *anchor, *refAnchor );
                }
            }
        }
    }

    return found;
}


int PNS_DIFF_PAIR_PLACER::viaGap() const
{
    return SizeSettings().DiffPairViaGap();
}


int PNS_DIFF_PAIR_PLACER::gap() const
{
    return SizeSettings().DiffPairGap() + SizeSettings().DiffPairWidth();
}


bool PNS_DIFF_PAIR_PLACER::Start( const VECTOR2I& aP, PNS_ITEM* aStartItem )
{
    ClearFailureReason();

    if( !aStartItem )
    {
        SetFailureReason( _( "Can't start a differential pair "
                             " in the middle of nowhere." ) );
        return false;
    }


    if( !findDpPrimitivePair( aP, aStartItem, m_start ) )
    {
        SetFailureReason( _( "Unable to find complementary differential pair "
                             "net. Make sure the names of the nets belonging "
                             "to a differential pair end with either _N/_P or +/-." ) );
        return false;
    }

    m_netP = m_start.PrimP()->Net();
    m_netN = m_start.PrimN()->Net();

    int clearance = std::min( SizeSettings().DiffPairGap(), SizeSettings().DiffPairViaGap() );
    if ( clearance > GetInitialWorld()->GetClearance( m_netP ) ||
         clearance > GetInitialWorld()->GetClearance( m_netN ) )
    {
        SetFailureReason( _( "Current track/via gap setting violates "
                             "design rules for this net." ) );
        return false;
    }

    // FIXME: It's not our business to validate the settings
    if( SizeSettings().DiffPairWidth() < SizeSettings().MinimumTrackWidth() )
    {
        SetFailureReason( _( "Current track width setting violates design rules." ) );
        return false;
    }

    m_currentStart = aP;
    m_currentEnd = aP;
    m_placingVia = false;
    m_chainedPlacement = false;

    initPlacement();

    return true;
}


void PNS_DIFF_PAIR_PLACER::initPlacement()
{
    m_idle = false;
    m_orthoMode = false;
    m_currentEndItem = NULL;
    m_startDiagonal = m_initialDiagonal;

    PNS_NODE* world = GetInitialWorld();

    world->KillChildren(); // Why is this needed?
    m_world = world->Branch();

    m_lastNode = NULL;
    m_currentNode = m_world;
    m_currentMode = RoutingSettings().Mode();

    if( m_shove )
        delete m_shove;

    m_shove = NULL;

    if( m_currentMode == RM_Shove || m_currentMode == RM_Smart )
    {
        m_shove = new PNS_SHOVE( m_currentNode );
    }
}

bool PNS_DIFF_PAIR_PLACER::routeHead( const VECTOR2I& aP )
{
    m_fitOk = false;

    PNS_DP_GATEWAYS gwsEntry( gap() );
    PNS_DP_GATEWAYS gwsTarget( gap() );

    if( !m_prevPair )
        m_prevPair = m_start;

    gwsEntry.BuildFromPrimitivePair( *m_prevPair, m_startDiagonal );

    PNS_DP_PRIMITIVE_PAIR target;

    if( findDpPrimitivePair ( aP, m_currentEndItem, target ) )
    {
        gwsTarget.BuildFromPrimitivePair( target, m_startDiagonal );
        m_snapOnTarget = true;
    } else {
        VECTOR2I fp;

        if( !propagateDpHeadForces( aP, fp ) )
            return false;

        gwsTarget.SetFitVias( m_placingVia, SizeSettings().ViaDiameter(), viaGap() );
        gwsTarget.BuildForCursor( fp );
        gwsTarget.BuildOrthoProjections( gwsEntry, fp, m_orthoMode ? 200 : -200 );
        m_snapOnTarget = false;
    }

    m_currentTrace = PNS_DIFF_PAIR();
    m_currentTrace.SetGap( gap() );
    m_currentTrace.SetLayer( m_currentLayer );

    if ( gwsEntry.FitGateways( gwsEntry, gwsTarget, m_startDiagonal, m_currentTrace ) )
    {
        m_currentTrace.SetNets( m_netP, m_netN );
        m_currentTrace.SetWidth( SizeSettings().DiffPairWidth() );
        m_currentTrace.SetGap( SizeSettings().DiffPairGap() );

        if( m_placingVia )
        {
            m_currentTrace.AppendVias ( makeVia ( m_currentTrace.CP().CPoint(-1), m_netP ),
                                        makeVia ( m_currentTrace.CN().CPoint(-1), m_netN ) );
        }

        return true;
    }

    return false;
}


bool PNS_DIFF_PAIR_PLACER::Move( const VECTOR2I& aP , PNS_ITEM* aEndItem )
{
    ClearFailureReason();
    m_currentEndItem = aEndItem;
    m_fitOk = false;

    delete m_lastNode;
    m_lastNode = NULL;

    if( !route( aP ) )
        return false;

    PNS_NODE* latestNode = m_currentNode;
    m_lastNode = latestNode->Branch();

    assert( m_lastNode != NULL );
    m_currentEnd = aP;

    updateLeadingRatLine();

    return true;
}


void PNS_DIFF_PAIR_PLACER::SizeSettingsChanged()
{
    if( !m_idle )
    {
        initPlacement();
        Move( m_currentEnd, NULL );
    }
}

bool PNS_DIFF_PAIR_PLACER::FixRoute( const VECTOR2I& aP, PNS_ITEM* aEndItem )
{
    ClearFailureReason();

    if( !m_fitOk )
        return false;

    if( m_currentTrace.CP().SegmentCount() < 1 ||
            m_currentTrace.CN().SegmentCount() < 1 )
        return false;

    if( m_currentTrace.CP().SegmentCount() > 1 )
        m_initialDiagonal = !DIRECTION_45( m_currentTrace.CP().CSegment( -2 ) ).IsDiagonal();

    PNS_TOPOLOGY topo( m_lastNode );

    if( !m_snapOnTarget && !m_currentTrace.EndsWithVias() )
    {
        SHAPE_LINE_CHAIN newP( m_currentTrace.CP() );
        SHAPE_LINE_CHAIN newN( m_currentTrace.CN() );

        if( newP.SegmentCount() > 1 && newN.SegmentCount() > 1 )
        {
            newP.Remove( -1, -1 );
            newN.Remove( -1, -1 );
        }

        m_currentTrace.SetShape( newP, newN );
    }

    if( m_currentTrace.EndsWithVias() )
    {
        m_lastNode->Add( m_currentTrace.PLine().Via().Clone() );
        m_lastNode->Add( m_currentTrace.NLine().Via().Clone() );
        m_chainedPlacement = false;
    }
    else
    {
        m_chainedPlacement = !m_snapOnTarget;
    }

    PNS_LINE lineP( m_currentTrace.PLine() );
    PNS_LINE lineN( m_currentTrace.NLine() );

    m_lastNode->Add( &lineP );
    m_lastNode->Add( &lineN );

    topo.SimplifyLine( &lineP );
    topo.SimplifyLine( &lineN );

    m_prevPair = m_currentTrace.EndingPrimitives();

    setResultingWorld( m_lastNode );

    m_lastNode = NULL;
    m_placingVia = false;

    if( m_snapOnTarget )
    {
        m_idle = true;
        return true;
    }
    else
    {
        initPlacement();
        return false;
    }
}


void PNS_DIFF_PAIR_PLACER::GetModifiedNets( std::vector<int> &aNets ) const
{
    aNets.push_back( m_netP );
    aNets.push_back( m_netN );
}


void PNS_DIFF_PAIR_PLACER::updateLeadingRatLine()
{
    SHAPE_LINE_CHAIN ratLineN, ratLineP;
    PNS_TOPOLOGY topo( m_lastNode );

    if( topo.LeadingRatLine( &m_currentTrace.PLine(), ratLineP ) )
    {
        DrawDebugLine( ratLineP, 1, 10000 );
    }

    if( topo.LeadingRatLine ( &m_currentTrace.NLine(), ratLineN ) )
    {
        DrawDebugLine( ratLineN, 3, 10000 );
    }
}


const std::vector<int> PNS_DIFF_PAIR_PLACER::CurrentNets() const
{
    std::vector<int> rv;
    rv.push_back( m_netP );
    rv.push_back( m_netN );
    return rv;
}
