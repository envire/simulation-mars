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
 * \file WorldPhysics.cpp
 * \author Malte Roemmermann
 * \brief "WorldPhysics" includes the methods to handle the physically world.
 *
 * Conditions:
 *           - The state of the private variable world_init is
 *             aquivalent to the initialization state of the world,
 *             space and the contactgroup variables
 *
 * ToDo:
 *               - get and set the standard physical parameters
 *               - get and set the special ODE parameters via
 *                 a generic component through the Simulator class
 *               - handle sensor data 
 *
 */

#include "WorldPhysics.h"
#include "NodePhysics.h"


#include <mars/utils/MutexLocker.h>
#include <mars/interfaces/graphics/draw_structs.h>
#include <mars/interfaces/graphics/GraphicsManagerInterface.h>
#include <mars/interfaces/sim/SimulatorInterface.h>
#include <mars/interfaces/Logging.hpp>

#include <mars/sim/SimNode.h>

#include <boost/scoped_ptr.hpp>
#include <boost/intrusive_ptr.hpp>	

#include <envire_core/graph/EnvireGraph.hpp>
#include <envire_core/items/Item.hpp>

#define SIM_CENTER_FRAME_NAME std::string("center")

#include <maps/grid/MLSMap.hpp>
#include <smurf/Collidable.hpp>
#define MLS_FRAME_NAME std::string("mls_01")
#define DEBUG_MARS 1


namespace mars {
  namespace sim {

    using namespace utils;
    using namespace interfaces;

    PhysicsError WorldPhysics::error = PHYSICS_NO_ERROR;

    void myMessageFunction(int errnum, const char *msg, va_list ap) {
      CPP_UNUSED(errnum);
      LOG_INFO(msg, ap);
    }

    void myDebugFunction(int errnum, const char *msg, va_list ap) {
      CPP_UNUSED(errnum);
      LOG_DEBUG(msg, ap);
      WorldPhysics::error = PHYSICS_DEBUG;
    }

    void myErrorFunction(int errnum, const char *msg, va_list ap) {
      CPP_UNUSED(errnum);
      LOG_ERROR(msg, ap);
      WorldPhysics::error = PHYSICS_ERROR;
    }

    /**
     *  \brief The constructor for the physical world.
     *
     *  pre:
     *      - none
     *
     *  post:
     *      - all private variables should be initialized correct
     *        should correct be spezified?
     *      - world, space, contactgroup and world_init to false (0)
     */
    WorldPhysics::WorldPhysics(ControlCenter *control) {

      this->control = control;
      draw_contact_points = 0;
      fast_step = 0;
      world_cfm = 1e-10;
      world_erp = 0.1;
      world_gravity = Vector(0.0, 0.0, -9.81);
      ground_friction = 20;
      ground_cfm = 0.00000001;
      ground_erp = 0.1;
      world = 0;
      space = 0;
      contactgroup = 0;
      world_init = 0;
      num_contacts = 0;
      create_contacts = 1;
      log_contacts = 0;

      // the step size in seconds
      step_size = 0.01;
      // dInitODE is relevant for using trimesh objects as correct as
      // possible in the ode implementation
      MutexLocker locker(&iMutex);
#ifdef ODE11
      // for ode-0.11
      dInitODE2(0);
      dAllocateODEDataForThread(dAllocateMaskAll);
#else
      dInitODE();
#endif
      dSetErrorHandler (myErrorFunction);
      dSetDebugHandler (myDebugFunction);
      dSetMessageHandler (myMessageFunction);
    }

    /**
     * \brief Close ODE environment
     *
     * pre:
     *     - none
     *
     * post:
     *     - everthing that was created should be destroyed
     *
     */
    WorldPhysics::~WorldPhysics(void) {
      // free the ode objects
      freeTheWorld();
      // and close the ODE ...
      MutexLocker locker(&iMutex);
      dCloseODE();
    }

    /**
     *  \brief This function initializes the ode world.
     *
     * pre:
     *     - world_init = false
     *
     * post:
     *     - world, space and contactgroup should be created
     *     - the ODE world parameters should be set
     *     - at the end world_init have to become true
     */
    void WorldPhysics::initTheWorld(void) {
      MutexLocker locker(&iMutex);
  
      // if world_init = true debug something
      if (!world_init) {
        //LOG_DEBUG("init physics world");
        world = dWorldCreate();
        space = dHashSpaceCreate(0);
        contactgroup = dJointGroupCreate(0);

        old_gravity = world_gravity;
        old_cfm = world_cfm;
        old_erp = world_erp;

        dWorldSetGravity(world, world_gravity.x(), world_gravity.y(), world_gravity.z()); 
        dWorldSetCFM(world, (dReal)world_cfm);
        dWorldSetERP (world, (dReal)world_erp);

        dWorldSetAutoDisableFlag (world,0);
        // if usefull for some tests a ground can be created here
        plane = 0; //dCreatePlane (space,0,0,1,0);
        world_init = 1;
        drawStruct draw;
        draw.ptr_draw = (DrawInterface*)this;
        if(control->graphics)
          control->graphics->addDrawItems(&draw);
      }
      //printf("initTheWorld..\n");
    }

    /**
     * \brief This functions destroys the ode world.
     *
     * pre:
     *     - world_init = true
     *
     * post:
     *     - world, space and contactgroup have to be destroyed here
     *     - afte that, world_init have to become false
     */
    void WorldPhysics::freeTheWorld(void) {
#ifdef DEBUG_MARS
      std::cout << "[WorldPhysics::freeTheWorld] START" << std::endl; 
#endif
      MutexLocker locker(&iMutex);
      if(world_init) {
        dJointGroupDestroy(contactgroup);
        dSpaceDestroy(space);
        dWorldDestroy(world);
        world_init = 0;
      }
#ifdef DEBUG_MARS
      std::cout << "[WorldPhysics::freeTheWorld] START" << std::endl; 
#endif
      // else debug something
    }

    /**
     * \brief Returns if a world exists.
     *
     * pre:
     *     - none
     *
     * post:
     *     - return state of world_init
     */
    bool WorldPhysics::existsWorld(void) const {
      return world_init;
    }

    /** 
     *
     * \brief Auxiliar methof of step the world. Checks required before
     * computing the collision and the contact forces are done here.
     *
     */
    void WorldPhysics::stepTheWorldChecks(void) {
        // Gravity Check
        if(old_gravity != world_gravity) {
          old_gravity = world_gravity;
          dWorldSetGravity(world, world_gravity.x(),
                           world_gravity.y(), world_gravity.z());
        }
        // CFM Check
        if(old_cfm != world_cfm) {
          old_cfm = world_cfm;
          dWorldSetCFM(world, (dReal)world_cfm);
        }
        // ERP Check
        if(old_erp != world_erp) {
          old_erp = world_erp;
          dWorldSetERP(world, (dReal)world_erp);
        }
    }

    /** 
     *
     * \brief Auxiliar methof of step the world. 
     * Clears the contact and contact feedback information from previous step.
     *
     */
    void WorldPhysics::clearPreviousStep(void){
#ifdef DEBUG_MARS
      std::cout << "[WorldPhysics::clearPreviousStep] START" << std::endl; 
#endif
      // Clear Previous Collisions
      //	printf("now WorldPhysics.cpp..stepTheWorld(void)....1 : dSpaceGetNumGeoms: %d\n",dSpaceGetNumGeoms(space)); 
      /// first clear the collision counters of all geoms
      int i;
      geom_data* data;
      for(i=0; i<dSpaceGetNumGeoms(space); i++) {
        data = (geom_data*)dGeomGetData(dSpaceGetGeom(space, i));
        data->num_ground_collisions = 0;
        data->contact_ids.clear();
        data->contact_points.clear();
        data->ground_feedbacks.clear();

        // Clear draw_intern 
        draw_intern.clear(); //TODO: Can we remove this?

        // Clear contacts
        dJointGroupEmpty(contactgroup);

      }
      std::vector<dJointFeedback*>::iterator iter;
      // Clear Previous Contact Feedback
      for(iter = contact_feedback_list.begin();
          iter != contact_feedback_list.end(); iter++) {
        free((*iter));
      }
      contact_feedback_list.clear();
#ifdef DEBUG_MARS
      std::cout << "[WorldPhysics::clearPreviousStep] END" << std::endl; 
#endif
    }

    /** 
     *
     * \brief Auxiliar methof of computeCollisions. 
     * Returns all frames that contain collidable objects 
     *
     */
    std::vector<envire::core::FrameId> WorldPhysics::getAllColFrames(void)
    {
      using CollisionType = smurf::Collidable;
      using CollisionItem = envire::core::Item<CollisionType>;
      using IterCollItem = envire::core::EnvireGraph::ItemIterator<CollisionItem>;
      // Find out the frames which contain a collidablem put them in a vector and pass it to the callback
      std::vector<envire::core::FrameId> colFrames;
      envire::core::EnvireGraph::vertex_iterator  it, end;
      std::tie(it, end) = control->graph->getVertices();
      for(; it != end; ++it)
      {
          // See if the vertex has collision objects
          IterCollItem itCols, endCols;
          std::tie(itCols, endCols) = control->graph->getItems<CollisionItem>(*it);
          if(itCols != endCols)
          {
              envire::core::FrameId colFrame = control->graph->getFrameId(*it);
              colFrames.push_back(colFrame);
#ifdef DEBUG_MARS
              std::cout << "Collision items found in frame " << colFrame << std::endl;
#endif
          }
      }
      return colFrames;
    }

    /** 
     *
     * \brief Auxiliar methof of step the world. 
     * Computes the collisions points
     *
     * Go through all the nodes of the graph, for each one that has a
     * collision object or is an MLS compute the collisions to others.
     * Unless they have same group id.
     *
     */
    void WorldPhysics::computeCollisions(void){
      /// first check for collisions
      num_contacts = log_contacts = 0;
      create_contacts = 1;
      /*
       * The collision between collidable objects is left for later. By now we
       * compute only the collision points between the MLS and each collision
       * objects
       */
      /*
      std::vector<smurf::Collidable> allColls = getAllCollidables();
      for(int i=0; i<allColls.size(); i++)
      {
        smurf::Collidable collObj1 = allColls[i];
        LOG_DEBUG("[WorldPhysics::computeCollisions] About to check collision of: " + collObj1.getName());
        for(int j=i+1; j<allColls.size(); j++)
        {
          smurf::Collidable collObj2 = allColls[j];
          if (collObj1.getGroupId()!=collObj2.getGroupId())
          {
            LOG_DEBUG("[WorldPhysics::computeCollisions] With: " + collObj2.getName());

          }
        }
      }
      */
      // Get the mls
      using mlsType = maps::grid::MLSMapPrecalculated;
      using CollisionType = smurf::Collidable;
      using CollisionItem = envire::core::Item<CollisionType>;
      using IterCollItem = envire::core::EnvireGraph::ItemIterator<CollisionItem>;
      envire::core::EnvireGraph::ItemIterator<envire::core::Item<mlsType>> beginItem, endItem;
      envire::core::FrameId mlsFrameId = MLS_FRAME_NAME;
      boost::tie(beginItem, endItem) = control->graph->getItems<envire::core::Item<mlsType>>(mlsFrameId);
      if (beginItem != endItem)
      {
        mlsType mls = beginItem->getData();
#ifdef DEBUG_MARS
        std::cout << "[WorldPhysics::computeCollision]: Mls map was fetched from the graph "<< std::endl;  
#endif
        // Get the frames that contain collidables
        std::vector<envire::core::FrameId> colFrames = getAllColFrames();
#ifdef DEBUG_MARS
        int countCollisions = 0;
#endif
        for(unsigned int frameIndex = 0; frameIndex<colFrames.size(); ++frameIndex)
        {
#ifdef DEBUG_MARS
          std::cout << "[WorldPhysics::computeCollision]: Collision related to frame " << colFrames[frameIndex] << std::endl;
#endif
          // Transformation must be from the mls frame to the colision object frame
          envire::core::Transform tfColCen = control->graph->getTransform(MLS_FRAME_NAME, colFrames[frameIndex]);
          fcl::Transform3f trafo = tfColCen.transform.getTransform().cast<float>();
          // Get the collision objects -Assumes only one per frame-
          IterCollItem itCols;
          itCols = control->graph->getItem<CollisionItem>(colFrames[frameIndex]); 
          smurf::Collidable collidable = itCols->getData();
          urdf::Collision collision = collidable.getCollision();
          // Prepare fcl call
          fcl::CollisionRequestf request(10, true, 10, true);
          fcl::CollisionResultf result;
          bool collisionComputed = true;
          switch (collision.geometry->type){
              case urdf::Geometry::SPHERE:
                  {
                      //std::cout << "Collision with a sphere" << std::endl;
                      boost::shared_ptr<urdf::Sphere> sphereUrdf = boost::dynamic_pointer_cast<urdf::Sphere>(collision.geometry);
                      fcl::Spheref sphere(sphereUrdf->radius);
                      fcl::collide_mls(mls, trafo, &sphere, request, result);
                      break;
                  }
              case urdf::Geometry::BOX:
                  {
                      //std::cout << "Collision with a box" << std::endl;
                      boost::shared_ptr<urdf::Box> boxUrdf = boost::dynamic_pointer_cast<urdf::Box>(collision.geometry);
                      fcl::Boxf box(boxUrdf->dim.x, boxUrdf->dim.y, boxUrdf->dim.z);
                      fcl::collide_mls(mls, trafo, &box, request, result);
                      break;
                  }
              default:
                  std::cout << "[WorldPhysics::computeCollision]: Collision with the selected geometry type not implemented" << std::endl;
                  collisionComputed = false;
          }
          if (collisionComputed)
          {
#ifdef DEBUG_MARS
            std::cout << "\n[WorldPhysics::computeCollision]: isCollision()==" << result.isCollision() << std::endl;
            if (result.isCollision())
            {
              std::cout << "\n [WorldPhysics::computeCollision]: Collision detected related to frame " << colFrames[frameIndex] << std::endl;
              countCollisions ++;
              // Here a method createContacts will put the joints that correspond
              createContacts(result, collidable, colFrames[frameIndex] ); 
              for(size_t i=0; i< result.numContacts(); ++i)
              {
                  const auto & cont = result.getContact(i);
                  std::cout << "[WorldPhysics::computeCollision]: Contact transpose " << cont.pos.transpose() << std::endl;
                  std::cout << "[WorldPhysics::computeCollision]: Contact normal transpose " << cont.normal.transpose() << std::endl;
                  std::cout << "[WorldPhysics::computeCollision]: Contact penetration depth " << cont.penetration_depth << std::endl;
              }
            }
#endif
          }
        }
#ifdef DEBUG_MARS
        std::cout << "Total collisions found " << countCollisions << std::endl; 
        std::cout << "Collision Check Finished " << std::endl;
#endif
      }
      else
      {
        std::cout << "No MLS found in frame "<< MLS_FRAME_NAME << std::endl;  
      }
    }

    void WorldPhysics::initContactParams(dContact *contactPtr, const smurf::ContactParams contactParams, int numContacts){
      //MLS Has currently no contact parameters, we will use just the ones of the collidable by now
      //const &geom_data1, const &geom_data2){
      // Make a method that initializes the parameters of the contacts, it is
      // what is done starting here until #END_INITCONTACPARAMS
      // frist we set the softness values:
      contactPtr[0].surface.mode = dContactSoftERP | dContactSoftCFM;
      contactPtr[0].surface.soft_cfm = contactParams.cfm;
#ifdef DEBUG_MARS
      std::cout << "[WorldPhysics::InitContactParameters] contactPtr[0].surface.soft_cfm " << contactPtr[0].surface.soft_cfm << std::endl;
      std::cout << "[WorldPhysics::InitContactParameters] ContactParams.cfm : " << contactParams.cfm <<std::endl;
      std::cout << "[WorldPhysics::InitContactParameters] ContactParams.friction1 : " << contactParams.friction1 <<std::endl;
      std::cout << "[WorldPhysics::InitContactParameters] ContactParams.friction1 : " << contactParams.friction_direction1 <<std::endl;
#endif
      contactPtr[0].surface.soft_erp = contactParams.erp;
      if(contactParams.approx_pyramid) 
      {
        contactPtr[0].surface.mode |= dContactApprox1;
      }                              
      contactPtr[0].surface.mu = contactParams.friction1;
      contactPtr[0].surface.mu2 = contactParams.friction2;
      if(contactPtr[0].surface.mu != contactPtr[0].surface.mu2)
        contactPtr[0].surface.mode |= dContactMu2;

      // Move handleFrictionDirection to another method
      // check if we have to calculate friction direction1
      if(contactParams.friction_direction1){
        std::cout << "[WorldPhysics::initiContactParams] About to set friction direction" << std::endl;
        dVector3 v1;
        contactPtr[0].surface.mode |= dContactFDir1;
        /*
         * Don't know how to do this part yet
         * TODO Improve based on what is Done in NearCallback
         *
         *
         */
        //v1[0] = geom_data1->c_params.friction_direction1->x();
        //v1[1] = geom_data1->c_params.friction_direction1->y();
        //v1[2] = geom_data1->c_params.friction_direction1->z();
        v1[0] = 0.0;
        v1[1] = 1.0;
        v1[2] = 0.0;
        // translate the friction direction to global coordinates
        // and set friction direction for contact
        //dMULTIPLY0_331(contact[0].fdir1, R, v1);
        contactPtr[0].fdir1[0] = v1[0];
        contactPtr[0].fdir1[1] = v1[1];
        contactPtr[0].fdir1[2] = v1[2];
        //if(geom_data1->c_params.motion1) {
        //  contactPtr[0].surface.mode |= dContactMotion1;
        //  contactPtr[0].surface.motion1 = geom_data1->c_params.motion1;
        //}
      }
      // then check for fds
      if(contactParams.fds1){
        contactPtr[0].surface.mode |= dContactSlip1;
        contactPtr[0].surface.slip1 = contactParams.fds1;
      }
      if(contactParams.fds2){
        contactPtr[0].surface.mode |= dContactSlip2;
        contactPtr[0].surface.slip2 = contactParams.fds2;
      }
      // Then set bounce and bounce_vel
      if(contactParams.bounce){
        contactPtr[0].surface.mode |= dContactBounce;
        contactPtr[0].surface.bounce = contactParams.bounce;
        contactPtr[0].surface.bounce_vel = contactParams.bounce_vel;
      }
      // Apply parametrization to all contacts.
      for (int i=1;i<numContacts;i++){
        contactPtr[i] = contactPtr[0];
      }
    }

    void WorldPhysics::createFeedbackJoints( const envire::core::FrameId frameId, const smurf::ContactParams contactParams, dContact *contactPtr, int numContacts){
#ifdef DEBUG_MARS
      std::cout << "[WorldPhysics::createFeedbackJoints] " << frameId << std::endl;
#endif
      //numContacts is the number of collisions detected by fcl between the robot and the mls
      //num_contacts is a global variable of Worldphysics to keep track of the existent feedback joints
      dVector3 v;
      //dMatrix3 R;
      dReal dot;
      num_contacts++;
      if(create_contacts){
        for(int i=0;i<numContacts;i++){
          if(contactParams.friction_direction1) {
            v[0] = contactPtr[i].geom.normal[0];
            v[1] = contactPtr[i].geom.normal[1];
            v[2] = contactPtr[i].geom.normal[2];
            dot = dDOT(v, contactPtr[i].fdir1);
            dOPEC(v, *=, dot);
            contactPtr[i].fdir1[0] -= v[0];
            contactPtr[i].fdir1[1] -= v[1];
            contactPtr[i].fdir1[2] -= v[2];
            dNormalize3(contactPtr[0].fdir1);
          }
          contactPtr[0].geom.depth += (contactParams.depth_correction);
          if(contactPtr[0].geom.depth < 0.0) contactPtr[0].geom.depth = 0.0;
          dJointID c=dJointCreateContact(world,contactgroup,contactPtr+i);
          // TODO !!! Here I have to attach it to the MLS somehow
          // I need to get the dGeomID of the objects
          // from the dGeomID I need to get the dBodyID
          //dBodyID b1=dGeomGetBody(o1);
          //dJointAttach(c,b1,0);
          envire::core::EnvireGraph::ItemIterator<envire::core::Item<std::shared_ptr<mars::sim::SimNode>>> begin, end;
          boost::tie(begin, end) = control->graph->getItems<envire::core::Item<std::shared_ptr<mars::sim::SimNode>>>(frameId);
          if (begin != end){
#ifdef DEBUG_MARS
            std::cout << "[WorldPhysics::createFeedbackJoints] We have the simnode! " << std::endl;
#endif            
            std::shared_ptr<mars::sim::SimNode> nodePtr = begin->getData();
            interfaces::NodeInterface * nodeIfPtr = nodePtr->getInterface();
            //interfaces::NodeInterface & nodeIfAdd = *nodeIfPtr;
            // TODO get the NodePhys out of the SimNode. The Node Physics has the method to fet the dBodyID, with it the dJointAttach method can be used
            //NodePhysics * nodePhys = dynamic_cast<NodePhysics*>(nodeIfPtr);
            //const dBodyID bodyId = nodePhys->getBody();
            

            dJointFeedback *fb;
            fb = (dJointFeedback*)malloc(sizeof(dJointFeedback));
            dJointSetFeedback(c, fb);
            contact_feedback_list.push_back(fb);

            Vector contact_point;
            contact_point.x() = contactPtr[0].geom.pos[0];
            contact_point.y() = contactPtr[0].geom.pos[1];
            contact_point.z() = contactPtr[0].geom.pos[2];
            
#ifdef DEBUG_MARS
            std::cout << "[WorldPhysics::addContact]: Contact point x" << contact_point.x() << std::endl;
            std::cout << "[WorldPhysics::addContact]: Contact point y" << contact_point.y() << std::endl;
            std::cout << "[WorldPhysics::addContact]: Contact point z" << contact_point.z() << std::endl;
#endif

            nodeIfPtr -> addContacts(c, numContacts, contactPtr[i], fb);
          }
        } // for numContacts
      } // if create contacts
#ifdef DEBUG_MARS
      std::cout << "[WorldPhysics::createFeedbackJoints] All done here " << std::endl;
#endif            
    }

      /*
      if(numc){ 
		  
	  
        if(create_contacts) {

          for(i=0;i<numc;i++){
            contact[0].geom.depth += (geom_data1->c_params.depth_correction +
                                      geom_data2->c_params.depth_correction);
        
            if(contact[0].geom.depth < 0.0) contact[0].geom.depth = 0.0;
            dJointID c=dJointCreateContact(world,contactgroup,contact+i);

            dJointAttach(c,b1,b2);
             ### Done until here ###

            geom_data1->num_ground_collisions += numc;
            geom_data2->num_ground_collisions += numc;

            contact_point.x() = contact[i].geom.pos[0];
            contact_point.y() = contact[i].geom.pos[1];
            contact_point.z() = contact[i].geom.pos[2];

            geom_data1->contact_ids.push_back(geom_data2->id);
            geom_data2->contact_ids.push_back(geom_data1->id);
            geom_data1->contact_points.push_back(contact_point);
            geom_data2->contact_points.push_back(contact_point);
            //if(dGeomGetClass(o1) == dPlaneClass) {
            fb = 0;
            if(geom_data2->sense_contact_force) {
              fb = (dJointFeedback*)malloc(sizeof(dJointFeedback));
              dJointSetFeedback(c, fb);
           
              contact_feedback_list.push_back(fb);
              geom_data2->ground_feedbacks.push_back(fb);
              geom_data2->node1 = false;
            } 
            //else if(dGeomGetClass(o2) == dPlaneClass) {
            if(geom_data1->sense_contact_force) {
              if(!fb) {
                fb = (dJointFeedback*)malloc(sizeof(dJointFeedback));
                dJointSetFeedback(c, fb);
                  
                contact_feedback_list.push_back(fb);
              }
              geom_data1->ground_feedbacks.push_back(fb);
              geom_data1->node1 = true;
            }
          }
        }  
      }
 *
 */


    void WorldPhysics::dumpFCLResult(const fcl::CollisionResultf &result, dContact *contactPtr)
    {
#ifdef DEBUG_MARS
      std::cout << "[WorldPhysics::dumpFCLResults] To Dump: " << std::endl;
      for(size_t i=0; i< result.numContacts(); ++i)
      {
          const auto & cont = result.getContact(i);
          std::cout << "[WorldPhysics::dumpFCLResults]: Contact transpose " << cont.pos.transpose() << std::endl;
          std::cout << "[WorldPhysics::dumpFCLResults]: Contact normal transpose " << cont.normal.transpose() << std::endl;
          std::cout << "[WorldPhysics::dumpFCLResults]: Contact penetration depth " << cont.penetration_depth << std::endl;
      }
#endif

#ifdef DEBUG_MARS
      std::cout << "[WorldPhysics::dumpFCLResults] Result: " << std::endl;

      dVector3 vNormal;
      Vector contact_point;
      for(size_t i=0; i< result.numContacts(); ++i)
      {
        contact_point.x() = contactPtr[0].geom.pos[0];
        contact_point.y() = contactPtr[0].geom.pos[1];
        contact_point.z() = contactPtr[0].geom.pos[2];
        vNormal[0] = contactPtr[i].geom.normal[0];
        vNormal[1] = contactPtr[i].geom.normal[1];
        vNormal[2] = contactPtr[i].geom.normal[2];
        const auto & cont = result.getContact(i);
        std::cout << "[WorldPhysics::dumpFCLResults]:  contactPtr[0].geom.pos" << contact_point << std::endl;
        std::cout << "[WorldPhysics::dumpFCLResults]: contactPtr[i].geom.normal " << vNormal << std::endl;
        std::cout << "[WorldPhysics::dumpFCLResults]: contactPtr[i].geom.depth " << contactPtr[i].geom.depth << std::endl;
      }
#endif
    }

    /** 
     *
     * \brief Method called in computeCollisions when collisions are found.
     * This method instantiates the correspondent contact joints.
     * The method is based on what nearCallback was doing
     */
    void WorldPhysics::createContacts(const fcl::CollisionResultf & result, smurf::Collidable collidable, const envire::core::FrameId frameId){
#ifdef DEBUG_MARS
      std::cout << "[WorldPhysics::CreateContacts] Collidable " << collidable.getName() << std::endl;
#endif
      // Init dContact
      dContact *contactPtr = new dContact[result.numContacts()];
      const smurf::ContactParams contactParams = collidable.getContactParams();
      initContactParams(contactPtr, contactParams, result.numContacts());
      dumpFCLResult(result, contactPtr);
      // Here we have to copy the contact points to the contactPtr structure or
      // if not pass the result to createFeedbackJoints so that it uses them
      createFeedbackJoints(frameId, contactParams, contactPtr, result.numContacts());
    }
    

    /** 
     *
     * \brief Auxiliar methof of step the world. 
     * Executes the world step
     *
     */
    void WorldPhysics::execStep(void){
        // then calculate the next state for a time of step_size seconds
        try {
          if(fast_step) dWorldQuickStep(world, step_size);
          else dWorldStep(world, step_size);

        } catch (...) {
          control->sim->handleError(PHYSICS_UNKNOWN);
        }
	if(WorldPhysics::error) {
          control->sim->handleError(WorldPhysics::error);
          WorldPhysics::error = PHYSICS_NO_ERROR;
	}
    }

    /**
     * \brief This function handles the calculation of a step in the world.
     *
     * pre:
     *     - world_init = true
     *     - step_size > 0
     *
     * post:
     *     - handled the collisions
     *     - step the world for step_size seconds
     *     - the contactgroup should be empty
     *
     * TODO: Refactor this method. Move the initial checks to a separate
     * method, the clears to another. The auxiliar vars initialization can also
     * be moved to the auxiliar methods
     */
    void WorldPhysics::stepTheWorld(void) {
      MutexLocker locker(&iMutex);
      // if world_init = false or step_size <= 0 debug something
      if (world_init && step_size > 0){
        stepTheWorldChecks();
        clearPreviousStep();
        // TODO This method should be much similar to the previous one running
        // dSpaceCollide and after computing all the "external contacts" those
        // which take place with the mls
        computeCollisions();
        // Update draw (I guess) //TODO: Can we remove all draw stuff?
        drawLock.lock();
        draw_extern.swap(draw_intern);
        drawLock.unlock();
        execStep();
      }   
      // When is nearCallBack executed??
    }

    /**
     * \brief Returns the ode ID of the world object.
     *
     * pre:
     *     - none
     *
     * post:
     *     - world ID returned
     */
    dWorldID WorldPhysics::getWorld(void) const {
      return world;
    }

    /**
     * \brief Returns the ode ID of the main space object.
     *
     * pre:
     *     - none
     *
     * post:
     *     - space ID returned
     */
    dSpaceID WorldPhysics::getSpace(void) const {
      return space;
    }

    /**
     * \brief Sets the body pointer param to the body for the comp_group_id
     *
     * The functions sets the body pointer to the body ID that
     * represents the composite object. If no body is aviable
     * for the composite group a body will be created.
     * The functions return if an body allready exists or if one
     * had been created.
     *
     * Careful with this function, bad implementation. This function should be
     * only called if a new geom will be conected to the given body.
     *
     * pre:
     *     - comp_group > 0
     *
     * post:
     *     - body pointer should be set to a regular body
     *       that is in the vector comp_body_list
     *     - retruned true if a body was created, otherwise retruned false
     */
    bool WorldPhysics::getCompositeBody(int comp_group, dBodyID* body,
                                        NodePhysics *node) {
      body_nbr_tupel tmp_tupel;

      // if comp_group is bad, debug something
      if(comp_group > 0) {
        std::vector<body_nbr_tupel>::iterator iter;
        for( iter = comp_body_list.begin(); iter != comp_body_list.end(); iter++ ) {
          if((*iter).comp_group == comp_group) {
            (*iter).connected_geoms++;
            *body = (*iter).body;
            (*iter).comp_nodes.push_back(node);
            return 0;
          }
        }
        tmp_tupel.body = *body = dBodyCreate(world);
        tmp_tupel.comp_group = comp_group;
        tmp_tupel.connected_geoms = 1;
        tmp_tupel.comp_nodes.push_back(node);
        comp_body_list.push_back(tmp_tupel);
        return 1;
      }
      return 0;
    }

    /**
     * \brief Destroyes a body from a node:
     *
     * This function checks if not more than one geom is connected to the
     * body and destroyes the body in that case. In the other case the counter
     * of the connected geoms is decreased.
     *
     * pre:
     *     - the body exists in the physical world
     *
     * post:
     *     - if more than one geoms are connected to the body, decrease the
     *       counter of connected geoms
     *     - otherwise, if only one geom is connected to the body, destroy the body
     */
    void WorldPhysics::destroyBody(dBodyID theBody, NodePhysics* node) {
      std::vector<body_nbr_tupel>::iterator iter;
      std::vector<NodePhysics*>::iterator jter;

      for(iter = comp_body_list.begin(); iter != comp_body_list.end(); iter++) {
        if((*iter).body == theBody) {
          if((*iter).connected_geoms > 1) {
            (*iter).connected_geoms--;
            for(jter = (*iter).comp_nodes.begin();
                jter != (*iter).comp_nodes.end(); jter++) {
              if((*jter) == node) {
                (*iter).comp_nodes.erase(jter);
                break;
              }
            }
            resetCompositeMass(theBody);
            return;
          }
          else {
            dBodyDestroy(theBody);
            comp_body_list.erase(iter);
            return;
          }
        }
      }
      // if we get here in the code, the body is not in the list and can
      // be removed from the world
      dBodyDestroy(theBody);
    }

    /**
     * \brief Returns the stepsize for calculating a world step
     *
     * pre:
     *     - none
     *
     * post:
     *     - the step_size value should be returned
     */
    dReal WorldPhysics::getWorldStep(void) {
      return step_size;
    }

    /**
     * \brief In this function the collision handling from ode is performed.
     *
     * pre:
     *     - world_init = true
     *     - o1 and o2 are regular geoms
     *
     * post:
     *     - if o1 or o2 was a Space, called SpaceCollide and exit
     *     - otherwise tested if the geoms collide and created a contact
     *       joint if so.
     *
     * A lot of the code is uncommented in this function. This
     * code maybe used later to handle sensors or other special cases
     * in the simulation.
     */
    void WorldPhysics::nearCallback (dGeomID o1, dGeomID o2) {
      int i;
      int numc;
      //up to MAX_CONTACTS contact per Box-box
      //dContact contact[MAX_CONTACTS];
      dVector3 v1, v;
      //dMatrix3 R;
      dReal dot;
  
      if (dGeomIsSpace(o1) || dGeomIsSpace(o2)) {
        /// test if a space is colliding with something
        dSpaceCollide2(o1,o2,this,& WorldPhysics::callbackForward);
        return;
      }

      dBodyID b1=dGeomGetBody(o1);
      dBodyID b2=dGeomGetBody(o2);

      geom_data* geom_data1 = (geom_data*)dGeomGetData(o1);
      geom_data* geom_data2 = (geom_data*)dGeomGetData(o2);


  
      // TODO Move all the ray sensor stuff to a separate method
      // handle ray sensor collisions
            // test if we have a ray sensor:
      if(geom_data1->ray_sensor) {
        dContact contact;
        if(geom_data1->parent_geom == o2) {
          return;
        }
        
        if(geom_data1->parent_body == dGeomGetBody(o2)) {
          return;
        }
        
        numc = dCollide(o2, o1, 1|CONTACTS_UNIMPORTANT, &(contact.geom), sizeof(dContact));
        if(numc) {
          if(contact.geom.depth < geom_data1->value)
            geom_data1->value = contact.geom.depth;
          ray_collision = 1;
        }
        return;
      }
      else if(geom_data2->ray_sensor) {
        dContact contact;
        if(geom_data2->parent_geom == o1) {
          return;
        }
        if(geom_data2->parent_body == dGeomGetBody(o1)) {
          return;
        }
        numc = dCollide(o2, o1, 1|CONTACTS_UNIMPORTANT, &(contact.geom), sizeof(dContact));
        if(numc) {
          if(contact.geom.depth < geom_data2->value)
            geom_data2->value = contact.geom.depth;
          ray_collision = 1;
        }
        return;
      }
      
      
      /// exit without doing anything if the two bodies are connected by a joint 
      if(b1 && b2 && dAreConnectedExcluding(b1,b2,dJointTypeContact))
        return;

      // The check of the ray_sensor does not make sense, because the case has
      // already been taken care of
      if(!b1 && !b2 && !geom_data1->ray_sensor && !geom_data2->ray_sensor) return;

      // Init dContact (move to another method, maxNumContacts has to be known
      // in this method later)
      int maxNumContacts = 0;
      if(geom_data1->c_params.max_num_contacts <
         geom_data2->c_params.max_num_contacts) {
        maxNumContacts = geom_data1->c_params.max_num_contacts;
      }
      else {
        maxNumContacts = geom_data2->c_params.max_num_contacts;
      }
      dContact *contact = new dContact[maxNumContacts];


      //for granular test
      //if( (plane != o2) && (plane !=o1)) return ;
  
  
      /*
     /// we use the geomData to handle some special cases
     void* geom_data1 = dGeomGetData(o1);
     void* geom_data2 = dGeomGetData(o2);

     /// one case is, that we don't wont to handle a collision between some special
     /// geoms beweet each other and the ground
     if((geom_data1 && ((robot_geom*)geom_data1)->type & 16)) {
     if(plane == o2) return;
     if((geom_data2 && ((robot_geom*)geom_data2)->type & 16)) return;
     }
     else if((geom_data2 && ((robot_geom*)geom_data2)->type & 16) && (plane == o1)) return;
  
     /// an other case is a ray geom that we use simulate ray sensors
     /// this geom has to be handled in a different way
     if((geom_data1 && ((robot_geom*)geom_data1)->type & 8) ||
     (geom_data2 && ((robot_geom*)geom_data2)->type & 8)) {    
     int n;
     const int N = MAX_CONTACTS;
     dContactGeom contact[N];

     n = dCollide (o2,o1,N,contact,sizeof(dContactGeom));
     if (n > 0) {
     //const dReal ss[3] = {1,0.01,0.01};
     for (i=0; i<n; i++) {
     contact[i].pos[2] += Z_OFFSET;
     if(contact[i].depth > 0.01){
     if(geom_data1 && ((robot_geom*)geom_data1)->type & 8)
     ((robot_geom*)geom_data1)->i_length = contact[0].depth;
     if(geom_data2 && ((robot_geom*)geom_data2)->type & 8)
     ((robot_geom*)geom_data2)->i_length = contact[0].depth;
     }
     }
     }
     return;
     }
      */
  

      // NOTE Here is where the joint for the contact is created
      //
      
      // Make a method that initializes the parameters of the contacts, it is
      // what is done starting here until #END_INITCONTACPARAMS
      // frist we set the softness values:
      contact[0].surface.mode = dContactSoftERP | dContactSoftCFM;
      contact[0].surface.soft_cfm = (geom_data1->c_params.cfm +
                                     geom_data2->c_params.cfm)/2;
      contact[0].surface.soft_erp = (geom_data1->c_params.erp +
                                     geom_data2->c_params.erp)/2;
      // then check if one of the geoms want to use the pyramid approximation
      if(geom_data1->c_params.approx_pyramid ||
         geom_data2->c_params.approx_pyramid)
        contact[0].surface.mode |= dContactApprox1;
  
      // Then check the friction for both directions
      contact[0].surface.mu = (geom_data1->c_params.friction1 +
                               geom_data2->c_params.friction1)/2;
      contact[0].surface.mu2 = (geom_data1->c_params.friction2 +
                                geom_data2->c_params.friction2)/2;

      if(contact[0].surface.mu != contact[0].surface.mu2)
        contact[0].surface.mode |= dContactMu2;

      // Move handleFrictionDirection to another method
      // check if we have to calculate friction direction1
      if(geom_data1->c_params.friction_direction1 ||
         geom_data2->c_params.friction_direction1) {
        // here the calculation becomes more complicated
        // maybe we should make some restrictions
        // so -> we only use friction motion in friction direction 1
        // the friction motion is only set if a local vector for friction
        // direction 1 is given
        // the steps for the calculation:
        // 1. rotate the local vectors to global coordinates
        // 2. scale the vectors to the length of the motion if given
        // 3. vector 3 =  vector 1 - vector 2
        // 4. get the length of vector 3
        // 5. set vector 3 as friction direction 1
        // 6. set motion 1 to the length
        contact[0].surface.mode |= dContactFDir1;
        if(!geom_data2->c_params.friction_direction1) {
          // get the orientation of the geom
          //dGeomGetQuaternion(o1, v);
          //dRfromQ(R, v);
          // copy the friction direction
          v1[0] = geom_data1->c_params.friction_direction1->x();
          v1[1] = geom_data1->c_params.friction_direction1->y();
          v1[2] = geom_data1->c_params.friction_direction1->z();
          // translate the friction direction to global coordinates
          // and set friction direction for contact
          //dMULTIPLY0_331(contact[0].fdir1, R, v1);
          contact[0].fdir1[0] = v1[0];
          contact[0].fdir1[1] = v1[1];
          contact[0].fdir1[2] = v1[2];
          if(geom_data1->c_params.motion1) {
            contact[0].surface.mode |= dContactMotion1;
            contact[0].surface.motion1 = geom_data1->c_params.motion1;
          }
        }
        else if(!geom_data1->c_params.friction_direction1) {
          // get the orientation of the geom
          //dGeomGetQuaternion(o2, v);
          //dRfromQ(R, v);
          // copy the friction direction
          v1[0] = geom_data2->c_params.friction_direction1->x();
          v1[1] = geom_data2->c_params.friction_direction1->y();
          v1[2] = geom_data2->c_params.friction_direction1->z();
          // translate the friction direction to global coordinates
          // and set friction direction for contact
          //dMULTIPLY0_331(contact[0].fdir1, R, v1);
          contact[0].fdir1[0] = v1[0];
          contact[0].fdir1[1] = v1[1];
          contact[0].fdir1[2] = v1[2];
          if(geom_data2->c_params.motion1) {
            contact[0].surface.mode |= dContactMotion1;
            contact[0].surface.motion1 = geom_data2->c_params.motion1;
          }
        }
        else {
          // the calculation steps as mentioned above
          fprintf(stderr, "the calculation for friction direction set for both nodes is not done yet.\n");
        }
      }

      // then check for fds
      if(geom_data1->c_params.fds1 || geom_data2->c_params.fds1) {
        contact[0].surface.mode |= dContactSlip1;
        contact[0].surface.slip1 = (geom_data1->c_params.fds1 +
                                    geom_data2->c_params.fds1);
      }
      if(geom_data1->c_params.fds2 || geom_data2->c_params.fds2) {
        contact[0].surface.mode |= dContactSlip2;
        contact[0].surface.slip2 = (geom_data1->c_params.fds2 +
                                    geom_data2->c_params.fds2);
      }

      // Then set bounce and bounce_vel
      if(geom_data1->c_params.bounce || geom_data2->c_params.bounce) {
        contact[0].surface.mode |= dContactBounce;
        contact[0].surface.bounce = (geom_data1->c_params.bounce +
                                     geom_data2->c_params.bounce);
        if(geom_data1->c_params.bounce_vel > geom_data2->c_params.bounce_vel)
          contact[0].surface.bounce_vel = geom_data1->c_params.bounce_vel;
        else
          contact[0].surface.bounce_vel = geom_data2->c_params.bounce_vel;      
      }
      
      // Apply parametrization to all contacts.
      for (i=1;i<maxNumContacts;i++){
        contact[i] = contact[0];
     
      }
      // #END_INITCONTACPARAMS
      
      // Most likely dCollide takes the two objects and determines the contact
      // points by calling the correspondent dCollideType1Type2 function NOTE
      // Where was for contact[0] the geom set? It isn't set, here you give the
      // memory address to set it!
      
#ifdef DEBUG_MARS
      // This lines is as today (7.09.2017) the method nearCallback is not
      // being called. What we could do is either replace the following methdod
      // dCollide for the collision computation in fcl and change back the code
      // so that this method is called or move what is done in this method each
      // time a collision is found in ComputeCollisions
      std::cout << "[WorldPhysics::nearCallback] FOOOOOO" << maxNumContacts << std::endl; 
#endif
      numc=dCollide(o1,o2, maxNumContacts, &contact[0].geom,sizeof(dContact));
    
      // Is dCollide the method to be replaced? This one provides the
      // information on where exactly the joints that apply the forces of the
      // colision should be set. This information we could get from FCL

      if(numc){ 
		  
	  
        dJointFeedback *fb;
        draw_item item;
        Vector contact_point;

        num_contacts++;
        if(create_contacts) {
          fb = 0;
          item.id = 0;
          item.type = DRAW_LINE;
          item.draw_state = DRAW_STATE_CREATE;
          item.point_size = 10;
          item.myColor.r = 1;
          item.myColor.g = 0;
          item.myColor.b = 0;
          item.myColor.a = 1;
          item.label = "";
          item.t_width = item.t_height = 0;
          item.texture = "";
          item.get_light = 0;

          for(i=0;i<numc;i++){
            item.start.x() = contact[i].geom.pos[0];
            item.start.y() = contact[i].geom.pos[1];
            item.start.z() = contact[i].geom.pos[2];
            // NOTE Where are the pos set? in the call to dCollide above in this same method
            item.end.x() = contact[i].geom.pos[0] + contact[i].geom.normal[0];
            item.end.y() = contact[i].geom.pos[1] + contact[i].geom.normal[1];
            item.end.z() = contact[i].geom.pos[2] + contact[i].geom.normal[2];
            draw_intern.push_back(item);
            if(geom_data1->c_params.friction_direction1 ||
               geom_data2->c_params.friction_direction1) {
              v[0] = contact[i].geom.normal[0];
              v[1] = contact[i].geom.normal[1];
              v[2] = contact[i].geom.normal[2];
              dot = dDOT(v, contact[i].fdir1);
              dOPEC(v, *=, dot);
              contact[i].fdir1[0] -= v[0];
              contact[i].fdir1[1] -= v[1];
              contact[i].fdir1[2] -= v[2];
              dNormalize3(contact[0].fdir1);
            }
            contact[0].geom.depth += (geom_data1->c_params.depth_correction +
                                      geom_data2->c_params.depth_correction);
        
            if(contact[0].geom.depth < 0.0) contact[0].geom.depth = 0.0;
            dJointID c=dJointCreateContact(world,contactgroup,contact+i);

            dJointAttach(c,b1,b2);

            geom_data1->num_ground_collisions += numc;
            geom_data2->num_ground_collisions += numc;

            contact_point.x() = contact[i].geom.pos[0];
            contact_point.y() = contact[i].geom.pos[1];
            contact_point.z() = contact[i].geom.pos[2];

            geom_data1->contact_ids.push_back(geom_data2->id);
            geom_data2->contact_ids.push_back(geom_data1->id);
            geom_data1->contact_points.push_back(contact_point);
            geom_data2->contact_points.push_back(contact_point);
            //if(dGeomGetClass(o1) == dPlaneClass) {
            fb = 0;
            if(geom_data2->sense_contact_force) {
              fb = (dJointFeedback*)malloc(sizeof(dJointFeedback));
              dJointSetFeedback(c, fb);
           
              contact_feedback_list.push_back(fb);
              geom_data2->ground_feedbacks.push_back(fb);
              geom_data2->node1 = false;
            } 
            //else if(dGeomGetClass(o2) == dPlaneClass) {
            if(geom_data1->sense_contact_force) {
              if(!fb) {
                fb = (dJointFeedback*)malloc(sizeof(dJointFeedback));
                dJointSetFeedback(c, fb);
                  
                contact_feedback_list.push_back(fb);
              }
              geom_data1->ground_feedbacks.push_back(fb);
              geom_data1->node1 = true;
            }
          }
        }  
      }
      delete[] contact;
    }

    /**
     * \brief This static function is used to project a normal function
     *   pointer to a method from a class
     *
     * pre:
     *     - data is a pointer to a correct object from type WorldPhysics
     *
     * post:
     *     - the newCallback method of the data object should be called
     */
    void WorldPhysics::callbackForward(void *data, dGeomID o1, dGeomID o2) {
      WorldPhysics *wp = (WorldPhysics*)data;
      wp->nearCallback(o1, o2);
    }

    /**
     * \brief resets the mass of a composite body
     *
     * pre:
     *     - the body exists in the physical world
     *
     * post:
     */
    void WorldPhysics::resetCompositeMass(dBodyID theBody) {
      std::vector<body_nbr_tupel>::iterator iter;
      std::vector<NodePhysics*>::iterator jter;
      dMass bodyMass, tmpMass;
      //bool first = 1;

      for(iter = comp_body_list.begin(); iter != comp_body_list.end(); iter++) {
        if((*iter).body == theBody) {      
          dMassSetZero(&bodyMass);
          for(jter = (*iter).comp_nodes.begin();
              jter != (*iter).comp_nodes.end(); jter++) {
            (*jter)->addMassToCompositeBody(theBody, &bodyMass);
          }
          dBodySetMass(theBody, &bodyMass);
          break;
        }
      }
    }

    void WorldPhysics::moveCompositeMassCenter(dBodyID theBody, dReal x,
                                               dReal y, dReal z) {
      std::vector<body_nbr_tupel>::iterator iter;
      std::vector<NodePhysics*>::iterator jter;
      const dReal *bpos;

      // first we have to calculate the offset in bodyframe
      // so rotate the vector
      bpos = dBodyGetPosition(theBody);
      dBodySetPosition(theBody, bpos[0]+x, bpos[1]+y, bpos[2]+z);
      for(iter = comp_body_list.begin(); iter != comp_body_list.end(); iter++) {
        if((*iter).body == theBody) {      
          for(jter = (*iter).comp_nodes.begin();
              jter != (*iter).comp_nodes.end(); jter++) {
            (*jter)->addCompositeOffset(-x, -y, -z);
          }
          break;
        }
      }
    }

    const Vector WorldPhysics::getCenterOfMass(const std::vector<NodeInterface*> &nodes) const {
      MutexLocker locker(&iMutex);
      Vector center;
      std::vector<NodeInterface*>::const_iterator iter;
      dMass sumMass;
      dMass tMass;

      dMassSetZero(&sumMass);
      for(iter = nodes.begin(); iter != nodes.end(); iter++) {
        ((NodePhysics*)(*iter))->getAbsMass(&tMass);
        dMassAdd(&sumMass, &tMass);
      }
      center.x() = sumMass.c[0];
      center.y() = sumMass.c[1];
      center.z() = sumMass.c[2];
      return center;
    }

    void WorldPhysics::update(std::vector<draw_item>* drawItems) {
      MutexLocker locker(&drawLock);
      std::vector<draw_item>::iterator iter;
  
      for(iter=drawItems->begin(); iter!=drawItems->end(); iter++) {
        iter->draw_state = DRAW_STATE_ERASE;
      }
      if(draw_contact_points) {
        for(iter=draw_extern.begin(); iter!=draw_extern.end(); iter++) {
          drawItems->push_back(*iter);
        }
      }
    }

    int WorldPhysics::handleCollision(dGeomID theGeom) {
      ray_collision = 0;
      dSpaceCollide2(theGeom, (dGeomID)space, this,
                     &WorldPhysics::callbackForward);
      return ray_collision;
    }

    double WorldPhysics::getCollisionDepth(dGeomID theGeom) {
      dGeomID otherGeom;
      dContact contact[1];
      double depth = 0.0;
      int numc;
      dBodyID b1;
      dBodyID b2;

      for(int i=0; i<dSpaceGetNumGeoms(space); i++) {
        otherGeom = dSpaceGetGeom(space, i);

        if(!(dGeomGetCollideBits(theGeom) & dGeomGetCollideBits(otherGeom)))
          continue;

        b1 = dGeomGetBody(theGeom);
        b2 = dGeomGetBody(otherGeom);

        if(b1 && b2 && dAreConnectedExcluding(b1,b2,dJointTypeContact))
          continue;

        numc = dCollide(theGeom, otherGeom, 1 | CONTACTS_UNIMPORTANT,
                        &(contact[0].geom), sizeof(dContact));
        if(numc) {
          if(contact[0].geom.depth > depth)
            depth = contact[0].geom.depth;
        }
      }

      return depth;
    }

    int WorldPhysics::checkCollisions(void) {
      MutexLocker locker(&iMutex);
      num_contacts = log_contacts = 0;
      create_contacts = 0;
      dSpaceCollide(space,this, &WorldPhysics::callbackForward);	
      return num_contacts;
    }

    double WorldPhysics::getVectorCollision(const Vector &pos, 
                                            const Vector &ray) const {
      MutexLocker locker(&iMutex);
      dGeomID otherGeom;
      dContact contact[1];
      //double depth = ray.length();
      double depth = ray.norm();
      int numc;
  
      dGeomID theGeom = dCreateRay(space, depth);
      dGeomRaySet(theGeom, pos.x(), pos.y(), pos.z(), ray.x(), ray.y(), ray.z()); 

      for(int i=0; i<dSpaceGetNumGeoms(space); i++) {
        otherGeom = dSpaceGetGeom(space, i);

        if(!(dGeomGetCollideBits(theGeom) & dGeomGetCollideBits(otherGeom)))
          continue;
        numc = dCollide(theGeom, otherGeom, 1 | CONTACTS_UNIMPORTANT,
                        &(contact[0].geom), sizeof(dContact));
        if(numc) {
          if(contact[0].geom.depth < depth)
            depth = contact[0].geom.depth;
        }
      }

      dGeomDestroy(theGeom);
      return depth;
    }

  } // end of namespace sim
} // end of namespace mars
