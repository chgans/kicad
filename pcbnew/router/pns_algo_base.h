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

#ifndef __PNS_ALGO_BASE_H
#define __PNS_ALGO_BASE_H

#include "pns_routing_settings.h"
#include "pns_sizes_settings.h"
#include "pns_meander.h" // for meander settings

class PNS_ROUTER_IFACE;
class PNS_LOGGER;

/**
 * Class PNS_ALGO_BASE
 *
 * Base class for all P&S algorithms (shoving, walkaround, line placement, dragging, etc.)
 * Holds a bunch of objects commonly used by all algorithms (P&S settings, parent router instance, logging)
 */
class PNS_ALGO_BASE
{
public:
    PNS_ALGO_BASE( PNS_ROUTER_IFACE* aRouter ) :
        m_router( aRouter )
    {}

    virtual ~PNS_ALGO_BASE() {}

    ///> Returns the instance of our router
    PNS_ROUTER_IFACE* Router() const
    {
        return m_router;
    }

    ///> Returns the logger object, allowing to dump geometry to a file.
    virtual PNS_LOGGER* Logger();

    /**
     * Function UpdateSizeSettings()
     *
     * Performs on-the-fly update of the width, via diameter & drill size from
     * a settings class. Used to dynamically change these parameters as
     * the track is routed.
     */
    void UpdateSizeSettings( const PNS_SIZES_SETTINGS& aSettings )
    {
        m_sizeSettings = aSettings;
        SizeSettingsChanged();
    }

    const PNS_SIZES_SETTINGS &SizeSettings() const
    {
        return m_sizeSettings;
    }

    void UpdateRoutingSettings( const PNS_ROUTING_SETTINGS& aSettings )
    {
        m_routingSettings = aSettings;
        RoutingSettingsChanged();
    }

    const PNS_ROUTING_SETTINGS &RoutingSettings() const
    {
        return m_routingSettings;
    }

    void UpdateMeanderSettings( const PNS_MEANDER_SETTINGS& aSettings )
    {
        m_meanderSettings = aSettings;
        MeanderSettingsChanged();
    }

    const PNS_MEANDER_SETTINGS &MeanderSettings() const
    {
        return m_meanderSettings;
    }

protected:
    virtual void RoutingSettingsChanged()
    {}

    virtual void SizeSettingsChanged()
    {}

    virtual void MeanderSettingsChanged()
    {}

private:
    PNS_ROUTER_IFACE* m_router;
    PNS_ROUTING_SETTINGS m_routingSettings;
    PNS_SIZES_SETTINGS m_sizeSettings;
    PNS_MEANDER_SETTINGS m_meanderSettings;
};

#endif
