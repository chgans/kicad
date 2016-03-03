/*
 * KiRouter - a push-and-(sometimes-)shove PCB router
 *
 * Copyright (C) 2014 CERN
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

#include "pns_item.h"
#include "pns_via.h"
#include "pns_solid.h"
#include "pns_node.h"
#include "pns_sizes_settings.h"

int PNS_SIZES_SETTINGS::inheritedTrackWidth( const PNS_ITEM* aItem )
{
    VECTOR2I p;

    assert( aItem->Owner() != NULL );

    switch( aItem->Kind() )
    {
    case PNS_ITEM::VIA:
        p = static_cast<const PNS_VIA*>( aItem )->Pos();
        break;

    case PNS_ITEM::SOLID:
        p = static_cast<const PNS_SOLID*>( aItem )->Pos();
        break;

    case PNS_ITEM::SEGMENT:
        return static_cast<const PNS_SEGMENT*>( aItem )->Width();

    default:
        return 0;
    }

    PNS_JOINT* jt = static_cast<PNS_NODE*>( aItem->Owner() )->FindJoint( p, aItem );

    assert( jt != NULL );

    int mval = INT_MAX;


    PNS_ITEMSET linkedSegs = jt->Links();
    linkedSegs.ExcludeItem( aItem ).FilterKinds( PNS_ITEM::SEGMENT );

    BOOST_FOREACH( PNS_ITEM* item, linkedSegs.Items() )
    {
        int w = static_cast<PNS_SEGMENT*>( item )->Width();
        mval = std::min( w, mval );
    }

    return ( mval == INT_MAX ? 0 : mval );
}


void PNS_SIZES_SETTINGS::ClearLayerPairs()
{
    m_layerPairs.clear();
}


void PNS_SIZES_SETTINGS::AddLayerPair( int aL1, int aL2 )
{
    int top = std::min( aL1, aL2 );
    int bottom = std::max( aL1, aL2 );

    m_layerPairs[bottom] = top;
    m_layerPairs[top] = bottom;
}


int PNS_SIZES_SETTINGS::GetLayerTop() const
{
    if( m_layerPairs.empty() )
        return F_Cu;
    else
        return m_layerPairs.begin()->first;
}


int PNS_SIZES_SETTINGS::GetLayerBottom() const
{
    if( m_layerPairs.empty() )
        return B_Cu;
    else
        return m_layerPairs.begin()->second;
}
