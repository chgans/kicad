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

#include "trace.h"

#include "pns_node.h"
#include "pns_itemset.h"
#include "pns_topology.h"
#include "pns_meander_skew_placer.h"


PNS_MEANDER_SKEW_PLACER::PNS_MEANDER_SKEW_PLACER () :
    PNS_MEANDER_PLACER ()
{
    // Init temporary variables (do not leave uninitialized members)
    m_coupledLength = 0;
}


PNS_MEANDER_SKEW_PLACER::~PNS_MEANDER_SKEW_PLACER( )
{
}


bool PNS_MEANDER_SKEW_PLACER::Start( const VECTOR2I& aP, PNS_ITEM* aStartItem )
{
    VECTOR2I p;

    ClearFailureReason();
    if( !aStartItem || !aStartItem->OfKind( PNS_ITEM::SEGMENT ) )
    {
        SetFailureReason( _( "Please select a differential pair trace you want to tune." ) );
        return false;
    }

    m_initialSegment = static_cast<PNS_SEGMENT*>( aStartItem );

    p = m_initialSegment->Seg().NearestPoint( aP );

    m_currentNode = NULL;
    m_currentStart = p;

    m_world = GetInitialWorld()->Branch();
    m_originLine = m_world->AssembleLine( m_initialSegment );

    PNS_TOPOLOGY topo( m_world );
    m_tunedPath = topo.AssembleTrivialPath( m_initialSegment );

    if( !topo.AssembleDiffPair ( m_initialSegment, m_originPair ) )
    {
        SetFailureReason( _( "Unable to find complementary differential pair "
                             "net for skew tuning. Make sure the names of the nets belonging "
                             "to a differential pair end with either _N/_P or +/-." ) );
        return false;
    }

    if( m_originPair.Gap() < 0 )
        m_originPair.SetGap( SizeSettings().DiffPairGap() );

    if( !m_originPair.PLine().SegmentCount() ||
        !m_originPair.NLine().SegmentCount() )
        return false;

    m_tunedPathP = topo.AssembleTrivialPath( m_originPair.PLine().GetLink( 0 ) );
    m_tunedPathN = topo.AssembleTrivialPath( m_originPair.NLine().GetLink( 0 ) );

    m_world->Remove( &m_originLine );

    m_currentWidth = m_originLine.Width();
    m_currentEnd = VECTOR2I( 0, 0 );

    if ( m_originPair.PLine().Net() == m_originLine.Net() )
        m_coupledLength = itemsetLength( m_tunedPathN );
    else
        m_coupledLength = itemsetLength( m_tunedPathP );

    return true;
}


int PNS_MEANDER_SKEW_PLACER::origPathLength( ) const
{
    return itemsetLength ( m_tunedPath );
}


int PNS_MEANDER_SKEW_PLACER::itemsetLength( const PNS_ITEMSET& aSet ) const
{
    int total = 0;
    BOOST_FOREACH( const PNS_ITEM* item, aSet.CItems() )
    {
        if( const PNS_LINE* l = pns_item_cast<const PNS_LINE*>( item ) )
        {
            total += l->CLine().Length();
        }
    }

    return total;
}


int PNS_MEANDER_SKEW_PLACER::currentSkew() const
{
    return m_lastLength - m_coupledLength;
}


bool PNS_MEANDER_SKEW_PLACER::Move( const VECTOR2I& aP, PNS_ITEM* aEndItem )
{
    ClearFailureReason();
	BOOST_FOREACH( const PNS_ITEM* item, m_tunedPathP.CItems() )
    {
        if( const PNS_LINE* l = pns_item_cast<const PNS_LINE*>( item ) )
            DrawDebugLine( l->CLine(), 5, 10000 );
    }

    BOOST_FOREACH( const PNS_ITEM* item, m_tunedPathN.CItems() )
    {
        if( const PNS_LINE* l = pns_item_cast<const PNS_LINE*>( item ) )
            DrawDebugLine( l->CLine(), 4, 10000 );
    }

    return doMove( aP, aEndItem, m_coupledLength + m_settings.m_targetSkew );
}

bool PNS_MEANDER_SKEW_PLACER::IsDual() const
{
    return false; // FIXME: Not true, needed for making up the current info/status text
}

int PNS_MEANDER_SKEW_PLACER::CurrentLength() const
{
    return currentSkew();
}

int PNS_MEANDER_SKEW_PLACER::TargetLength() const
{
    return m_settings.m_targetSkew;
}

int PNS_MEANDER_SKEW_PLACER::PairGap() const
{
    return 0;
}
