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

#ifndef __PNS_ROUTER_H
#define __PNS_ROUTER_H

#include <list>

#include <boost/optional.hpp>
#include <boost/unordered_set.hpp>

#include <geometry/shape_line_chain.h>
#include <class_undoredo_container.h>

#include "pns_routing_settings.h"
#include "pns_sizes_settings.h"
#include "pns_item.h"
#include "pns_itemset.h"
#include "pns_node.h"

class BOARD;
class BOARD_ITEM;
class D_PAD;
class TRACK;
class VIA;
class GRID_HELPER;
class PNS_NODE;
class PNS_DIFF_PAIR_PLACER;
class PNS_PLACEMENT_ALGO;
class PNS_LINE_PLACER;
class PNS_ITEM;
class PNS_LINE;
class PNS_SOLID;
class PNS_SEGMENT;
class PNS_JOINT;
class PNS_VIA;
class PNS_CLEARANCE_FUNC;
class PNS_PCBNEW_CLEARANCE_FUNC;
class PNS_SHOVE;
class PNS_DRAGGER;

namespace KIGFX
{
    class VIEW;
    class VIEW_GROUP;
};


enum PNS_ROUTER_MODE {
    PNS_MODE_ROUTE_SINGLE = 1,
    PNS_MODE_ROUTE_DIFF_PAIR,
    PNS_MODE_TUNE_SINGLE,
    PNS_MODE_TUNE_DIFF_PAIR,
    PNS_MODE_TUNE_DIFF_PAIR_SKEW
};

class PNS_ROUTER_IFACE
{
public:
    PNS_ROUTER_IFACE()
    {}

    virtual ~PNS_ROUTER_IFACE()
    {}

    // Node tree access, used by placers.
    // Is it a good idea to be able to do: placer->setWorld()
    virtual PNS_NODE* GetWorld() const = 0;

    // Algo results, called by the various FixRoute(). Used to notify outside world what have changed?
    virtual void CommitRouting( PNS_NODE* aNode ) = 0;

    // Snapping (Only used by line and pair placers in their Start() )
    virtual bool SnappingEnabled() const = 0;
    virtual const VECTOR2I SnapToItem( PNS_ITEM* aItem, VECTOR2I aP, bool& aSplitsSegment ) = 0;

    // Clearance
    // Only used by pair placer to override temporally the clearance (!?!)
    // The clearance functor is transferred the the world node.
    virtual PNS_CLEARANCE_FUNC* GetClearanceFunc() const = 0;
    // Needed only by pair placer, should be part of the clearance functor
    // Clearance functor to be made a querying/validating class, eg: PNS_DESIGN_RULES_VALIDATOR/CHECKER or PNS_DRC_ENGINE
    virtual bool ValidateClearanceForNet( int aClearance, int aNet ) const = 0;

    // Differential pair (given a net, find its paired net)
    // For this one, it is obviously implementation dependent. This is about queryin the
    // netlist for specail cases. Diff pair are such special cases.
    virtual int DpCoupledNet( int aNet ) const = 0;
    virtual int DpNetPolarity( int aNet ) const = 0;
    virtual bool IsPairedNet( int aNet ) const = 0;
    virtual int PairingPolarity( int aNet ) const = 0;
    virtual int GetPairedNet( int aNet ) const = 0;

    // Debugging
    virtual void DisplayDebugLine( const SHAPE_LINE_CHAIN& aLine, int aType = 0, int aWidth = 0 ) = 0;
    virtual void DisplayDebugPoint( const VECTOR2I aPos, int aType = 0 ) = 0;
    virtual void DrawDebugPoint( VECTOR2I aP, int aColor ) = 0;
    virtual void DrawDebugBox( BOX2I aB, int aColor ) = 0;
    virtual void DrawDebugSeg( SEG aS, int aColor ) = 0;
    virtual void DrawDebugDirs( VECTOR2D aP, int aMask, int aColor ) = 0;
};

/**
 * Class PNS_ROUTER
 *
 * Main router class.
 */
class PNS_ROUTER: public PNS_ROUTER_IFACE
{
private:
    enum RouterState
    {
        IDLE,
        DRAG_SEGMENT,
        ROUTE_TRACK
    };

public:
    PNS_ROUTER();
    virtual ~PNS_ROUTER();

    void SetMode ( PNS_ROUTER_MODE aMode );
    PNS_ROUTER_MODE Mode() const { return m_mode; }

    void ClearWorld();
    void SetBoard( BOARD* aBoard );
    void SyncWorld();

    void SetView( KIGFX::VIEW* aView );

    bool RoutingInProgress() const;
    bool StartRouting( const VECTOR2I& aP, PNS_ITEM* aItem, int aLayer );
    void Move( const VECTOR2I& aP, PNS_ITEM* aItem );
    bool FixRoute( const VECTOR2I& aP, PNS_ITEM* aItem );

    void StopRouting();

    void FlipPosture();

    void DisplayItem( const PNS_ITEM* aItem, int aColor = -1, int aClearance = -1 );
    void DisplayItems( const PNS_ITEMSET& aItems );

    void SwitchLayer( int layer );

    void ToggleViaPlacement();
    void SetOrthoMode ( bool aEnable );

    int GetCurrentLayer() const;
    const std::vector<int> GetCurrentNets() const;

    void DumpLog();

    bool IsPlacingVia() const;

    const PNS_ITEMSET   QueryHoverItems( const VECTOR2I& aP );

    bool StartDragging( const VECTOR2I& aP, PNS_ITEM* aItem );

    /**
     * Returns the last changes introduced by the router (since the last time ClearLastChanges()
     * was called or a new track has been started).
     */
    const PICKED_ITEMS_LIST& GetUndoBuffer() const
    {
        return m_undoBuffer;
    }

    /**
     * Clears the list of recent changes, saved to be stored in the undo buffer.
     */
    void ClearUndoBuffer()
    {
        m_undoBuffer.ClearItemsList();
    }

    /**
     * Applies stored settings.
     * @see Settings()
     */
    void UpdateSizes( const PNS_SIZES_SETTINGS& aSizes );

    /**
     * Changes routing settings to ones passed in the parameter.
     * @param aSettings are the new settings.
     */
    void LoadSettings( const PNS_ROUTING_SETTINGS& aSettings )
    {
        m_settings = aSettings;
    }

    void EnableSnapping( bool aEnable );


    PNS_ITEM *QueryItemByParent ( const BOARD_ITEM *aItem ) const;

    BOARD *GetBoard();

    PNS_PLACEMENT_ALGO *Placer() { return m_placer; }

    void SetGrid( GRID_HELPER *aGridHelper )
    {
        m_gridHelper = aGridHelper;
    }

    // Error management
    void SetFailureReason ( const wxString& aReason );
    const wxString& FailureReason() const;

    /*
     * PNS_ROUTER_IFACE implementation
     */

    // Node tree access
    virtual PNS_NODE* GetWorld() const;

    // Algo results
    virtual void CommitRouting( PNS_NODE* aNode );

    // Snapping
    virtual bool SnappingEnabled() const;
    virtual const VECTOR2I SnapToItem( PNS_ITEM* aItem, VECTOR2I aP, bool& aSplitsSegment );

    // Clearance
    virtual PNS_CLEARANCE_FUNC* GetClearanceFunc() const;
    virtual bool ValidateClearanceForNet( int aClearance, int aNet ) const;

    // Differential pair
    virtual int DpCoupledNet( int aNet ) const;
    virtual int DpNetPolarity( int aNet ) const;
    virtual bool IsPairedNet( int aNet ) const;
    virtual int PairingPolarity( int aNet ) const;
    virtual int GetPairedNet( int aNet ) const;

    // Settings
    virtual PNS_ROUTING_SETTINGS& Settings();
    virtual PNS_SIZES_SETTINGS& Sizes();

    // Debugging
    virtual void DisplayDebugLine( const SHAPE_LINE_CHAIN& aLine, int aType = 0, int aWidth = 0 );
    virtual void DisplayDebugPoint( const VECTOR2I aPos, int aType = 0 );
    virtual void DrawDebugPoint( VECTOR2I aP, int aColor );
    virtual void DrawDebugBox( BOX2I aB, int aColor );
    virtual void DrawDebugSeg( SEG aS, int aColor );
    virtual void DrawDebugDirs( VECTOR2D aP, int aMask, int aColor );

private:
    void movePlacing( const VECTOR2I& aP, PNS_ITEM* aItem );
    void moveDragging( const VECTOR2I& aP, PNS_ITEM* aItem );

    void eraseView();
    void updateView( PNS_NODE* aNode, PNS_ITEMSET& aCurrent );

    void clearViewFlags();

    int MatchDpSuffix( wxString aNetName, wxString& aComplementNet, wxString& aBaseDpName ) const;

    // optHoverItem queryHoverItemEx(const VECTOR2I& aP);

    PNS_ITEM* pickSingleItem( PNS_ITEMSET& aItems ) const;
    void splitAdjacentSegments( PNS_NODE* aNode, PNS_ITEM* aSeg, const VECTOR2I& aP );

    PNS_ITEM* syncPad( D_PAD* aPad );
    PNS_ITEM* syncTrack( TRACK* aTrack );
    PNS_ITEM* syncVia( VIA* aVia );

    void commitPad( PNS_SOLID* aPad );
    void commitSegment( PNS_SEGMENT* aTrack );
    void commitVia( PNS_VIA* aVia );

    void highlightCurrent( bool enabled );

    void markViolations( PNS_NODE* aNode, PNS_ITEMSET& aCurrent, PNS_NODE::ITEM_VECTOR& aRemoved );

    RouterState m_state;

    BOARD* m_board;
    PNS_NODE* m_world;
    PNS_PLACEMENT_ALGO * m_placer;
    PNS_DRAGGER* m_dragger;

    KIGFX::VIEW* m_view;
    KIGFX::VIEW_GROUP* m_previewItems;

    bool m_snappingEnabled;

    PNS_ROUTING_SETTINGS m_settings;
    PNS_PCBNEW_CLEARANCE_FUNC* m_clearanceFunc;

    boost::unordered_set<BOARD_CONNECTED_ITEM*> m_hiddenItems;

    ///> Stores list of modified items in the current operation
    PICKED_ITEMS_LIST m_undoBuffer;
    PNS_SIZES_SETTINGS m_sizes;
    PNS_ROUTER_MODE m_mode;

    wxString m_failureReason;

    GRID_HELPER *m_gridHelper;
};

#endif
