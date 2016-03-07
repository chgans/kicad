/*
 * KiRouter - a push-and-(sometimes-)shove PCB router
 *
 * Copyright (C) 2013-2014 CERN
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

#ifndef __PNS_PLACEMENT_ALGO_H
#define __PNS_PLACEMENT_ALGO_H

#include <math/vector2d.h>

#include "pns_algo_base.h"
#include "pns_itemset.h"
#include "pns_segment.h"

#include <wx/translation.h> // FIXME: _(message) is needed by various placement algo implementations

class PNS_DEBUG_DECORATOR
{
public:
    PNS_DEBUG_DECORATOR()
    {}
    virtual ~PNS_DEBUG_DECORATOR()
    {}

    virtual void AddPoint( VECTOR2I aP, int aColor ) = 0;
    virtual void AddLine( const SHAPE_LINE_CHAIN& aLine, int aType = 0, int aWidth = 0 ) = 0;
    virtual void AddSegment( SEG aS, int aColor ) = 0;
    virtual void AddBox( BOX2I aB, int aColor ) = 0;
    virtual void AddDirections( VECTOR2D aP, int aMask, int aColor ) = 0;
    virtual void Clear() = 0;
};

class PNS_ITEM;
class PNS_NODE;

/**
 * Class PNS_PLACEMENT_ALGO
 *
 * Abstract class for a P&S placement/dragging algorithm.
 * All subtools (drag, single/diff pair routing and meandering)
 * are derived from it.
 */

class PNS_PLACEMENT_ALGO : public PNS_ALGO_BASE
{
public:
    PNS_PLACEMENT_ALGO() :
        PNS_ALGO_BASE(), m_initialWorld( NULL ), m_resultingWorld( NULL )
    {}

    virtual ~PNS_PLACEMENT_ALGO () {}

    /**
     * Function SetInitialWorld
     *
     * Set the world this algorithm will work on.
     *
     * @todo param should be const
     */
    void SetInitialWorld( /*const*/ PNS_NODE *aWorld)
    {
        m_initialWorld = aWorld;
    }

    /**
     * Function GetResultingWorld
     *
     * Returns the resulting world this algorithm made.
     * Valid only after fixRoute() has deen called.
     *
     * @todo Should returns const
     */
    /*const*/ PNS_NODE *GetResultingWorld() const
    {
        return m_resultingWorld;
    }

    /**
     * Function Start()
     *
     * Starts placement/drag operation at point aP, taking item aStartItem as anchor
     * (unless NULL).
     * @return true on success and false in case of failure (use failureReason() for details)
     */
    virtual bool Start( const VECTOR2I& aP, PNS_ITEM* aStartItem ) = 0;

    /**
     * Function Move()
     *
     * Moves the end of the currently routed primtive(s) to the point aP, taking
     * aEndItem as the anchor (if not NULL).
     * (unless NULL).
     * @return true on success and false in case of failure (use failureReason() for details)
     */
    virtual bool Move( const VECTOR2I& aP, PNS_ITEM* aEndItem ) = 0;

    /**
     * Function FixRoute()
     *
     *
     * Finishes up the currently routed primitives, making the resulting world
     * ready for commit.
     * @return true if route can be commited without violating the DRC.
     */
    virtual bool FixRoute( const VECTOR2I& aP, PNS_ITEM* aEndItem ) = 0;

    /**
      * Function FailureReason()
      *
      * Returns a text message explaining why Start(), move() or FixRoute() failed.
      */
    wxString FailureReason() const
    {
        return m_failureReason;
    }

    /**
     * Function ToggleVia()
     *
     * Enables/disables a via at the end of currently routed trace.
     */
    virtual bool ToggleVia( bool aEnabled )
    {
        return false;
    }

    /**
     * Function IsPlacingVia()
     *
     * Returns true if the placer is placing a via (or more vias).
     */
    virtual bool IsPlacingVia() const
    {
        return false;
    }

    /**
     * Function SetLayer()
     *
     * Sets the current routing layer.
     */
    virtual bool SetLayer( int aLayer )
    {
        return false;
    }

    /**
     * Function Traces()
     *
     * Returns all routed/tuned traces.
     */
    virtual const PNS_ITEMSET Traces() = 0;

    /**
     * Function CurrentEnd()
     *
     * Returns the current end of the line(s) being placed/tuned. It may not be equal
     * to the cursor position due to collisions.
     */
    virtual const VECTOR2I& CurrentEnd() const = 0;

    /**
     * Function CurrentNets()
     *
     * Returns the net code(s) of currently routed track(s).
     */
    virtual const std::vector<int> CurrentNets() const = 0;

    /**
     * Function CurrentLayer()
     *
     * Returns the layer of currently routed track.
     */
    virtual int CurrentLayer() const = 0;

    /**
     * Function CurrentNode()
     *
     * Returns the most recent board state.
     */
    virtual PNS_NODE* CurrentNode( bool aLoopsRemoved = false ) const = 0;

    /**
     * Function FlipPosture()
     *
     * Toggles the current posture (straight/diagonal) of the trace head.
     */
    virtual void FlipPosture()
    {
    }

    /**
     * Function SetOrthoMode()
     *
     * Forces the router to place a straight 90/45 degree trace (with the end
     * as near to the cursor as possible) instead of a standard 135 degree
     * two-segment bend.
     */
    virtual void SetOrthoMode ( bool aOrthoMode )
    {
    }

    /**
     * Function GetModifiedNets
     *
     * Returns the net codes of all currently routed trace(s)
     */
    virtual void GetModifiedNets( std::vector<int> &aNets ) const
    {
    }

    /**
     * Function SetDebugDecorator
     *
     * Assign a debug decorator allowing this algo to draw extra graphics for visual debugging
     */
    void SetDebugDecorator( PNS_DEBUG_DECORATOR *aDecorator )
    {
        m_debugDecorator = aDecorator;
    }


protected:
    void SetFailureReason(const wxString &aReason)
    {
        m_failureReason = aReason;
    }

    void ClearFailureReason()
    {
        m_failureReason.Clear();
    }

    /*const*/ PNS_NODE *GetInitialWorld() const
    {
        return m_initialWorld;
    }

    void setResultingWorld( /*const*/ PNS_NODE *aWorld )
    {
        m_resultingWorld = aWorld;
    }

    ///> returm true if aItem is a segment item and aAnchor is on one of its extremities
    bool IsOnSegmentExtremities( const VECTOR2I& aAnchor, PNS_ITEM* aItem )
    {
        if (!aItem || aItem->Kind() != PNS_ITEM::SEGMENT)
            return false;

        PNS_SEGMENT* segmentItem = static_cast<PNS_SEGMENT*>( aItem );
        const SEG& segment = segmentItem->Seg();

        return (aAnchor == segment.A || aAnchor == segment.B );
    }

    // TODO: ensure not NULL by using a null decorator per default
    void DrawDebugPoint( VECTOR2I aP, int aColor )
    {
        if( m_debugDecorator )
            m_debugDecorator->AddPoint( aP, aColor );
    }

    void DrawDebugLine( const SHAPE_LINE_CHAIN& aLine, int aType = 0, int aWidth = 0 )
    {
        if( m_debugDecorator )
            m_debugDecorator->AddLine( aLine, aType, aWidth );
    }

    void DrawDebugSegment( SEG aS, int aColor )
    {
        if( m_debugDecorator )
            m_debugDecorator->AddSegment( aS, aColor );
    }

    void DrawDebugBox( BOX2I aB, int aColor )
    {
        if( m_debugDecorator )
            m_debugDecorator->AddBox( aB, aColor );
    }

    void DrawDebugDirections( VECTOR2D aP, int aMask, int aColor )
    {
        if( m_debugDecorator )
            m_debugDecorator->AddDirections( aP, aMask, aColor );
    }

    void ClearDebugDecorator()
    {
        if( m_debugDecorator )
            m_debugDecorator->Clear();
    }

private:
    wxString m_failureReason;
    /*const*/ PNS_NODE *m_initialWorld;
    /*const*/ PNS_NODE *m_resultingWorld;
    PNS_DEBUG_DECORATOR *m_debugDecorator;
};

#endif

