/*
 *  Copyright 2011, 2012, DFKI GmbH Robotics Innovation Center
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
 * \file TreeMarsInterface.h
 * \author Yong-Ho Yoo \n
 * \brief "TreeMarsInterface" declares the interfaces for all ItemOperations
 * that are used for the communication between the simulation modules.
 *
 * \version 1.0
 * \date 20.08.2015
 */

#ifndef TREE_MARS_INTERFACE_H
#define TREE_MARS_INTERFACE_H

#ifdef _PRINT_HEADER_
  #warning "TreeMarsInterface.h"
#endif

#include <string>
#include <mars/interfaces/MARSDefs.h>
#include <envire_core/TransformTree.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
namespace envire { namespace core
{ 
  class Transform; 
}}

namespace urdf
{
  class ModelInterface;
}

namespace configmaps
{
  class ConfigMap;
}

namespace mars { namespace interfaces 
{
  class NodeData;

  //Never use vertex_descriptor directly as it might change in the future
  //vertex_descriptor is only visible here because c++ lacks a good way to
  //encapsulate it.
  typedef envire::core::TransformTree::vertex_descriptor NodeIdentifier;

  class TreeMarsInterface {
  public:
    virtual ~TreeMarsInterface() {}

    /**Adds an object to the tree and adds a visual representation
     * (including physics) of the object to the mars scene graph.
     *
     * @param name The name of the frame that will be inserted into the tree
     * @param data The data that should be inserted into the frame
     * @param location Location of the object. I.e. transformation from the
     *                 parent node to the object.
     * @param parent The parent node.
     * @return An object that can be used to identify the node that was just created.
     */
    virtual NodeIdentifier addObject(const std::string& name,
                                     const mars::interfaces::NodeData& node,
                                     const envire::core::Transform& location,
                                     const NodeIdentifier& parent) = 0;
    /**Updates the physical state of the simulated items and
     * their corresponding vertices.
     * I.e. updates the nodes values from the physical layer
     * and copies position and rotation over to the corresponding edge
     * in the tree.
    */
    virtual void updateItemDynamics(sReal calc_ms, bool physics_thread) = 0;

    //draws the current tree structure to a dot file */
    virtual void drawDotFile(const std::string& file) const = 0;

    // Generate the nodes according to a SMURF Model
    virtual void loadRobot(boost::shared_ptr<urdf::ModelInterface> modelInterface,
                           const configmaps::ConfigMap& map) = 0;

    virtual NodeIdentifier getRoot() const = 0;

};
}} // end of namespace mars::interfaces

#endif  // TREE_MARS_INTERFACE_H
