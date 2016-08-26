/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
* Copyright 2008-2009 Pelican Ventures, Inc.
* http://osgearth.org
*
* osgEarth is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/


#include <osg/Notify>
#include <osgGA/StateSetManipulator>
#include <osgGA/GUIEventHandler>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgEarth/Map>
#include <osgEarth/MapNode>
#include <osgEarthUtil/EarthManipulator>
#include <osgEarthUtil/Viewpoint>


//// some preset viewpoints.
//static osgEarthUtil::Viewpoint VPs[] = {
//    osgEarthUtil::Viewpoint( "Africa",        osg::Vec3d( 115.0,   45.0, 0.0 ), 0.0, -25.0, 2000 ),
//    osgEarthUtil::Viewpoint( "California",    osg::Vec3d( -121.0,  34.0, 0.0 ), 0.0, -45.0, 6e6 ),
//    osgEarthUtil::Viewpoint( "Europe",        osg::Vec3d(    0.0,  45.0, 0.0 ), 0.0, -45.0, 4e6 ),
//    osgEarthUtil::Viewpoint( "Washington DC", osg::Vec3d(  -77.0,  38.0, 0.0 ), 0.0, -45.0, 1e6 ),
//    osgEarthUtil::Viewpoint( "Australia",     osg::Vec3d(  135.0, -20.0, 0.0 ), 0.0, -45.0, 2e6 )
//};
//
//// a simple handler that demonstrates the "viewpoint" functionality in 
//// osgEarthUtil::EarthManipulator. Press a number key to fly to a viewpoint.
//struct FlyToViewpointHandler : public osgGA::GUIEventHandler 
//{
//    FlyToViewpointHandler( osgEarthUtil::EarthManipulator* manip ) : _manip(manip) { }
//
//    bool handle( const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa )
//    {
//        if ( ea.getEventType() == ea.KEYDOWN && ea.getKey() >= '1' && ea.getKey() <= '5' )
//        {
//            _manip->setViewpoint( VPs[ea.getKey()-'1'], 4.0 );
//        }
//        return false;
//    }
//
//    osg::observer_ptr<osgEarthUtil::EarthManipulator> _manip;
//};

int main(int argc, char** argv)
{
    osg::ArgumentParser arguments(&argc,argv);

    osgViewer::Viewer viewer(arguments);

    // install the programmable manipulator.
	osgEarthUtil::EarthManipulator* manip = new osgEarthUtil::EarthManipulator();
    viewer.setCameraManipulator( manip );

    // The "Map" is the data model object that we will be visualizing. It will be
    // geocentric by default, but you can specify a projected map in the constructor.
    osgEarth::Map* map = new osgEarth::Map();

    // Add an image layer to the map.
    {
        osgEarth::Properties conf;
        //conf["url"] = "http://demo.pelicanmapping.com/rmweb/data/bluemarble-tms/tms.xml";
        conf["url"] = "http://readymap.org/readymap/tiles/1.0.0/7/";
		//conf["url"] = "F:/osgearth/data/cache1/NASA/tms.xml";
		osgEarth::MapLayer* layer = new osgEarth::MapLayer( "NASA", osgEarth::MapLayer::TYPE_IMAGE, "tms", conf );
        map->addMapLayer( layer );
    }

    // Add a heightfield layer to the map. You can add any number of heightfields and
    // osgEarth will composite them automatically.
    {
        osgEarth::Properties conf;
        //conf["url"] = "http://demo.pelicanmapping.com/rmweb/data/srtm30_plus_tms/tms.xml";
		conf["url"] = "http://readymap.org/readymap/tiles/1.0.0/9/";
		//conf["url"] = "F:/osgearth/data/cache1/srtm/tms.xml";
        osgEarth::MapLayer* layer = new osgEarth::MapLayer( "SRTM", osgEarth::MapLayer::TYPE_HEIGHTFIELD, "tms", conf );
        map->addMapLayer( layer );
    }

    // The MapNode will render the Map object in the scene graph.
	osgEarth::MapEngineProperties engineProps;
	engineProps.setVerticalScale(3.0);
    osgEarth::MapNode* mapNode = new osgEarth::MapNode( map,engineProps);

    viewer.setSceneData( mapNode );

    manip->getSettings()->bindMouseDoubleClick(
        osgEarthUtil::EarthManipulator::ACTION_GOTO,
        osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON );

    // add some stock OSG handlers:
    viewer.addEventHandler(new osgViewer::StatsHandler());
    viewer.addEventHandler(new osgViewer::WindowSizeHandler());
    viewer.addEventHandler(new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet()));

    return viewer.run();
}
