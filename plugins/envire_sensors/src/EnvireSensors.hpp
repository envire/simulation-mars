/*
 *  Copyright 2013, DFKI GmbH Robotics Innovation Center
 *
 *  This file is part of the MARS simulation framework.
 *
 *  MARS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3
 *  of the License, or (at your option) any later version.
 *
 *  MARS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with MARS.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * \file EnvireSensors.h
 * \author Raul (Raul.Dominguez@dfki.de)
 * \brief sensors-plugin-envire-mars
 *
 * Version 0.1
 */

#ifndef MARS_PLUGINS_ENVIRE_SENSORS_H
#define MARS_PLUGINS_ENVIRE_SENSORS_H

#ifdef _PRINT_HEADER_
  #warning "EnvireSensors.hpp"
#endif

// set define if you want to extend the gui
//#define PLUGIN_WITH_MARS_GUI
#include <mars/interfaces/sim/MarsPluginTemplate.h>
#include <mars/interfaces/MARSDefs.h>
#include <mars/interfaces/sensor_bases.h>
//#include <mars/data_broker/ReceiverInterface.h>
//#include <mars/cfg_manager/CFGManagerInterface.h>

#include <envire_core/events/GraphEventDispatcher.hpp>
#include <envire_core/events/GraphItemEventDispatcher.hpp>
#include <envire_core/graph/TreeView.hpp>
#include <envire_core/items/Item.hpp>

#include <smurf/Sensor.hpp>
#include <smurf/Frame.hpp>

#include <string>

namespace mars {

  namespace plugins {
    namespace envire_sensors {

      // inherit from MarsPluginTemplateGUI for extending the gui
      class EnvireSensors: public mars::interfaces::MarsPluginTemplate, 
                           public envire::core::GraphEventDispatcher,
                           public envire::core::GraphItemEventDispatcher<envire::core::Item<smurf::Sensor>>
      {

      public:
        EnvireSensors(lib_manager::LibManager *theManager);
        ~EnvireSensors();

        // LibInterface methods
        int getLibVersion() const
        { return 1; }
        const std::string getLibName() const
        { return std::string("envire_sensors"); }
        CREATE_MODULE_INFO();

        // MarsPlugin methods
        void init();
        void reset();
        void update(mars::interfaces::sReal time_ms);

        void display_position_data(const std::shared_ptr<mars::interfaces::BaseSensor>& sensorPtr );
        void display_rotatingRay_data(const std::shared_ptr<mars::interfaces::BaseSensor>& sensorPtr );
            
        // EnvireSensors methods

        void itemAdded(const envire::core::TypedItemAddedEvent<envire::core::Item<smurf::Sensor>>& e);        
        
      private:
        bool debug = true;
        unsigned long next_sensor_id = 0;
        
      }; // end of class definition EnvireSensors

    } // end of namespace envire_sensors
  } // end of namespace plugins
} // end of namespace mars

#endif // MARS_PLUGINS_ENVIRE_SENSORS_H
