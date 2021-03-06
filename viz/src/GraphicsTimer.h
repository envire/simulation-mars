/*
 *  Copyright 2013 DFKI GmbH Robotics Innovation Center
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

#ifndef MARS_VIZ_GRAPHICSTIMER_H
#define MARS_VIZ_GRAPHICSTIMER_H

#include <QObject>
#include <QTimer>

#include <mars/interfaces/graphics/GraphicsManagerInterface.h>

namespace mars {
  namespace viz {

    class GraphicsTimer : public QObject {
      Q_OBJECT;

    public:
      GraphicsTimer(mars::interfaces::GraphicsManagerInterface *graphics_);

      ~GraphicsTimer() {
        graphicsTimer->stop();
      }

      void run();
      void runOnce();
      void stop();
    signals:
      void internalRun();

    public slots:
      void timerEvent(void);
      void runOnceInternal(void);

    private:
      QTimer *graphicsTimer;
      mars::interfaces::GraphicsManagerInterface *graphics;
      bool runFinished;

    }; // end of class GraphicsTimer

  } // end of namespace viz
} // end of namespace mars

#endif /* MARS_VIZ_GRAPHICSTIMER_H */
