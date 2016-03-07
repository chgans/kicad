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

#ifndef __PNS_VIA_H
#define __PNS_VIA_H

#include <geometry/shape_line_chain.h>
#include <geometry/shape_circle.h>

#include "pns_item.h"

class PNS_NODE;

enum PNS_VIA_TYPE
{
    PNS_THROUGH_VIA = 3,      /* Always a through hole via */
    PNS_BURIED_VIA  = 2,      /* this via can be on internal layers */
    PNS_MICRO_VIA   = 1,      /* this via which connect from an external layer
                               * to the near neighbor internal layer */
};

class PNS_VIA : public PNS_ITEM
{
public:
    PNS_VIA() :
        PNS_ITEM( VIA )
    {
        m_diameter = 2;     // Dummy value
        m_drill = 0;
        m_viaType = PNS_THROUGH_VIA;
    }

    PNS_VIA( const VECTOR2I& aPos, const PNS_LAYERSET& aLayers,
             int aDiameter, int aDrill, int aNet = -1, PNS_VIA_TYPE aViaType = PNS_THROUGH_VIA ) :
        PNS_ITEM( VIA )
    {
        SetNet( aNet );
        SetLayers( aLayers );
        m_pos = aPos;
        m_diameter = aDiameter;
        m_drill = aDrill;
        m_shape = SHAPE_CIRCLE( aPos, aDiameter / 2 );
        m_viaType = aViaType;

        //If we're a through-board via, use all layers regardless of the set passed
        // FIXME: This is user dependent in several ways:
        //  - layer numbering (used to use KiCad's MAX_CU_LAYERS - 1)
        //  - Via type is not really used by PNS, this is the only exception here
        //  - => need for a VIA Factory?
        if( aViaType == PNS_THROUGH_VIA )
        {
            SetLayers( PNS_LAYERSET::All() );
        }
    }


    PNS_VIA( const PNS_VIA& aB ) :
        PNS_ITEM( VIA )
    {
        SetNet( aB.Net() );
        SetLayers( aB.Layers() );
        m_pos = aB.m_pos;
        m_diameter = aB.m_diameter;
        m_shape = SHAPE_CIRCLE( m_pos, m_diameter / 2 );
        m_marker = aB.m_marker;
        m_rank = aB.m_rank;
        m_drill = aB.m_drill;
        m_viaType = aB.m_viaType;
    }

    static inline bool ClassOf( const PNS_ITEM* aItem )
    {
        return aItem && VIA == aItem->Kind();
    }


    const VECTOR2I& Pos() const
    {
        return m_pos;
    }

    void SetPos( const VECTOR2I& aPos )
    {
        m_pos = aPos;
        m_shape.SetCenter( aPos );
    }

    PNS_VIA_TYPE ViaType() const
    {
        return m_viaType;
    }

    void SetViaType( PNS_VIA_TYPE aViaType )
    {
        m_viaType = aViaType;
    }

    int Diameter() const
    {
        return m_diameter;
    }

    void SetDiameter( int aDiameter )
    {
        m_diameter = aDiameter;
        m_shape.SetRadius( m_diameter / 2 );
    }

    int Drill() const
    {
        return m_drill;
    }

    void SetDrill( int aDrill )
    {
        m_drill = aDrill;
    }

    bool PushoutForce( PNS_NODE* aNode,
            const VECTOR2I& aDirection,
            VECTOR2I& aForce,
            bool aSolidsOnly = true,
            int aMaxIterations = 10 );

    const SHAPE* Shape() const
    {
        return &m_shape;
    }

    PNS_VIA* Clone() const;

    const SHAPE_LINE_CHAIN Hull( int aClearance = 0, int aWalkaroundThickness = 0 ) const;

    virtual VECTOR2I Anchor( int n ) const
    {
        return m_pos;
    }

    virtual int AnchorCount() const
    {
        return 1;
    }

    OPT_BOX2I ChangedArea( const PNS_VIA* aOther ) const;

private:
    int m_diameter;
    int m_drill;
    VECTOR2I m_pos;
    SHAPE_CIRCLE m_shape;
    PNS_VIA_TYPE m_viaType;
};

#endif
