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

#include <osgEarth/SpatialReference>
#include <osgEarth/Registry>
#include <osgEarth/Cube>
#include <ScopedLock>
#include <osg/Notify>
#include <ogr_api.h>
#include <ogr_spatialref.h>
#include <algorithm>

using namespace osgEarth;


static std::string
getOGRAttrValue( void* _handle, const std::string& name, int child_num, bool lowercase =false)
{
    GDAL_SCOPED_LOCK;
	const char* val = OSRGetAttrValue( _handle, name.c_str(), child_num );
    if ( val )
    {
        std::string t = val;
        if ( lowercase )
        {
            std::transform( t.begin(), t.end(), t.begin(), ::tolower );
        }
        return t;
    }
    return "";
}


SpatialReference::SpatialReferenceCache& SpatialReference::getSpatialReferenceCache()
{
    //Make sure the registry is created before the cache
    osgEarth::Registry::instance();
    static SpatialReferenceCache s_cache;
    return s_cache;
}


SpatialReference*
SpatialReference::createFromPROJ4( const std::string& init, const std::string& init_alias, const std::string& name )
{
    SpatialReference* result = NULL;
    GDAL_SCOPED_LOCK;
	void* handle = OSRNewSpatialReference( NULL );
    if ( OSRImportFromProj4( handle, init.c_str() ) == OGRERR_NONE )
	{
        result = new SpatialReference( handle, "PROJ4", init_alias, name );
	}
	else 
	{
        osg::notify(osg::WARN) << "[osgEarth::SRS] Unable to create spatial reference from PROJ4: " << init << std::endl;
		OSRDestroySpatialReference( handle );
	}
    return result;
}

SpatialReference*
SpatialReference::createCube(unsigned int face)
{
    std::string init = "+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs";

    SpatialReference* result = NULL;
    GDAL_SCOPED_LOCK;
	void* handle = OSRNewSpatialReference( NULL );
    if ( OSRImportFromProj4( handle, init.c_str() ) == OGRERR_NONE )
	{
        result = new CubeFaceSpatialReference( handle, face );
	}
	else 
	{
		osg::notify(osg::WARN) << "[osgEarth::SRS] Unable to create spatial reference from PROJ4: " << init << std::endl;
		OSRDestroySpatialReference( handle );
	}
    return result;
}

SpatialReference*
SpatialReference::createFromWKT( const std::string& init, const std::string& init_alias, const std::string& name )
{
    osg::ref_ptr<SpatialReference> result;
    GDAL_SCOPED_LOCK;
	void* handle = OSRNewSpatialReference( NULL );
    char buf[4096];
    char* buf_ptr = &buf[0];
	strcpy( buf, init.c_str() );
	if ( OSRImportFromWkt( handle, &buf_ptr ) == OGRERR_NONE )
	{
        result = new SpatialReference( handle, "WKT", init_alias, name );
        result = result->validate();
	}
	else 
	{
		osg::notify(osg::WARN) << "[osgEarth::SRS] Unable to create spatial reference from WKT: " << init << std::endl;
		OSRDestroySpatialReference( handle );
	}
    return result.release();
}

SpatialReference*
SpatialReference::create( const std::string& init )
{
    std::string low = init;
    std::transform( low.begin(), low.end(), low.begin(), ::tolower );

    SpatialReferenceCache::iterator itr = getSpatialReferenceCache().find(init);
    if (itr != getSpatialReferenceCache().end())
    {
        //osg::notify(osg::NOTICE) << "Returning cached SRS" << std::endl;
        return itr->second.get();
    }

    osg::ref_ptr<SpatialReference> srs;

    // shortcut for spherical-mercator:
    if (low == "spherical-mercator" || low == "epsg:900913" || low == "epsg:3785" || low == "epsg:41001" || low == "epsg:102113" )
    {
        // note the use of nadgrids=@null (see http://proj.maptools.org/faq.html)
        // adjusted +a by ONE to work around osg manipulator error until we can figure out why.. GW
        srs = createFromPROJ4(
            "+proj=merc +a=6378137 +b=6378137 +lon_0=0 +k=1 +x_0=0 +y_0=0 +nadgrids=@null +units=m +no_defs",
            init,
            "Spherical Mercator" );
    }

    // ellipsoidal ("world") mercator:
    else if (low == "epsg:54004" || low == "epsg:9804" || low == "epsg:3832")
    {
        srs = createFromPROJ4(
            "+proj=merc +lon_0=0 +k=1 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs",
            init,
            "World Mercator" );
    }

    // common WGS84:
    else if (low == "epsg:4326" || low == "wgs84")
    {
        srs = createFromPROJ4(
            "+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs",
            init,
            "WGS84" );
    }

    // custom square polar projection for cube rendering:
    else if ( ( low.size() == 11 ) && (low.substr(0,10) == "world-cube") )
    {
        //Try to extract a face from the string.
        unsigned int face = atoi(&low[10]);
        srs = createCube( face );
    }

    else if ( low.find( "+" ) == 0 )
    {
        srs = createFromPROJ4( low, init );
    }
    else if ( low.find( "epsg:" ) == 0 || low.find( "osgeo:" ) == 0 )
    {
        srs = createFromPROJ4( "+init=" + low, init );
    }
    else if ( low.find( "projcs" ) == 0 || low.find( "geogcs" ) == 0 )
    {
        srs = createFromWKT( init, init );
    }
    else
    {
        return NULL;
    }

    getSpatialReferenceCache()[init] = srs;
    return srs.get();
}


SpatialReference*
SpatialReference::create( osg::CoordinateSystemNode* csn )
{
    SpatialReference* result = NULL;
    if ( csn && !csn->getCoordinateSystem().empty() )
    {
        result = create( csn->getCoordinateSystem() );
    }
    return result;
}


static std::string&
replaceIn( std::string& s, const std::string& sub, const std::string& other)
{
    if ( sub.empty() ) return s;
    size_t b=0;
    for( ; ; )
    {
        b = s.find( sub, b );
        if ( b == s.npos ) break;
        s.replace( b, sub.size(), other );
        b += other.size();
    }
    return s;
}

//std::string
//SpatialReference::getAttributeValue( const std::string& name ) const
//{
//    return getOGRAttrValue( _handle, name, 0 );
//}

SpatialReference*
SpatialReference::validate()
{
    std::string proj = getOGRAttrValue( _handle, "PROJECTION", 0 );

    // fix invalid ESRI LCC projections:
    if ( proj == "Lambert_Conformal_Conic" )
    {
        bool has_2_sps =
            !getOGRAttrValue( _handle, "Standard_Parallel_2", 0 ).empty() ||
            !getOGRAttrValue( _handle, "standard_parallel_2", 0 ).empty();

        std::string new_wkt = getWKT();
        if ( has_2_sps )
            replaceIn( new_wkt, "Lambert_Conformal_Conic", "Lambert_Conformal_Conic_2SP" );
        else
            replaceIn( new_wkt, "Lambert_Conformal_Conic", "Lambert_Conformal_Conic_1SP" );

        osg::notify(osg::NOTICE) << "[osgEarth] Morphing Lambert_Conformal_Conic to 1SP/2SP" << std::endl;
        
        return createFromWKT( new_wkt, _init_str, _name );
    }

    // fixes for ESRI Plate_Carree and Equidistant_Cylindrical projections:
    else if ( proj == "Plate_Carree" )
    {
        std::string new_wkt = getWKT();
        replaceIn( new_wkt, "Plate_Carree", "Equirectangular" );
        osg::notify(osg::NOTICE) << "[osgEarth::SRS] Morphing Plate_Carree to Equirectangular" << std::endl;
        return createFromWKT( new_wkt, _init_str, _name ); //, input->getReferenceFrame() );
    }
    else if ( proj == "Equidistant_Cylindrical" )
    {
        std::string new_wkt = getWKT();
        osg::notify(osg::NOTICE) << "[osgEarth::SRS] Morphing Equidistant_Cylindrical to Equirectangular" << std::endl;
        replaceIn( new_wkt, "Equidistant_Cylindrical", "Equirectangular" );
        return createFromWKT( new_wkt, _init_str, _name );
    }

    // no changes.
    return this;
}


/****************************************************************************/


SpatialReference::SpatialReference(void* handle, 
                                   const std::string& init_type,
                                   const std::string& init_str,
                                   const std::string& name ) :
_handle( handle ),
_init_type( init_type ),
_init_str( init_str ),
_owns_handle( true ),
_name( name ),
_initialized( false )
{
    //TODO
    setThreadSafeReferenceCounting(true);
    _init_str_lc = init_str;
    std::transform( _init_str_lc.begin(), _init_str_lc.end(), _init_str_lc.begin(), ::tolower );
}

SpatialReference::SpatialReference(void* handle) :
_handle( handle ),
_owns_handle( true ),
_initialized( false )
{
    setThreadSafeReferenceCounting(true);
    init();
    _init_type = "WKT";
    _init_str = getWKT();
}

SpatialReference::~SpatialReference()
{
	if ( _handle && _owns_handle )
	{
      GDAL_SCOPED_LOCK;

      for (TransformHandleCache::iterator itr = _transformHandleCache.begin(); itr != _transformHandleCache.end(); ++itr)
      {
          OCTDestroyCoordinateTransformation(itr->second);
      }

      OSRDestroySpatialReference( _handle );

      
	}
	_handle = NULL;
}

bool
SpatialReference::isGeographic() const 
{
    if ( !_initialized )
        const_cast<SpatialReference*>(this)->init();
    return _is_geographic;
}

bool
SpatialReference::isProjected() const
{
    if ( !_initialized )
        const_cast<SpatialReference*>(this)->init();
    return !_is_geographic;
}

const std::string&
SpatialReference::getName() const
{
    if ( !_initialized )
        const_cast<SpatialReference*>(this)->init();
    return _name;
}

const osg::EllipsoidModel*
SpatialReference::getEllipsoid() const
{
    if ( !_initialized )
        const_cast<SpatialReference*>(this)->init();
    return _ellipsoid.get();
}

const std::string&
SpatialReference::getWKT() const 
{
    if ( !_initialized )
        const_cast<SpatialReference*>(this)->init();
    return _wkt;
}

const std::string&
SpatialReference::getInitString() const
{
    return _init_str;
}

const std::string&
SpatialReference::getInitType() const
{
    return _init_type;
}

bool
SpatialReference::isEquivalentTo( const SpatialReference* rhs ) const
{
    if ( !_initialized )
        const_cast<SpatialReference*>(this)->init();

    if ( !rhs )
        return false;

    if ( this == rhs )
        return true;

    const CubeFaceSpatialReference* cube_lhs = dynamic_cast< const CubeFaceSpatialReference*>(this);
    const CubeFaceSpatialReference* cube_rhs = dynamic_cast< const CubeFaceSpatialReference*>(rhs);

    if (cube_lhs && !cube_rhs) return false;
    if (cube_rhs && !cube_lhs) return false;
    if (cube_lhs && cube_rhs)
    {
        return cube_lhs->getFace() == cube_rhs->getFace();
    }

    if ( _init_str_lc == rhs->_init_str_lc )
        return true;

    if ( this->getWKT() == rhs->getWKT() )
        return true;

    if (this->isGeographic() && rhs->isGeographic() &&
        this->getEllipsoid()->getRadiusEquator() == rhs->getEllipsoid()->getRadiusEquator() &&
        this->getEllipsoid()->getRadiusPolar() == rhs->getEllipsoid()->getRadiusPolar())
    {
        return true;
    }

    // last resort, since it requires the lock
    GDAL_SCOPED_LOCK;
    return TRUE == ::OSRIsSame( _handle, rhs->_handle );
}

const SpatialReference*
SpatialReference::getGeographicSRS() const
{
    if ( !_initialized )
        const_cast<SpatialReference*>(this)->init();

    if ( _is_geographic )
        return this;

    if ( !_geo_srs.valid() )
    {
        GDAL_SCOPED_LOCK;

        void* new_handle = OSRNewSpatialReference( NULL );
        int err = OSRCopyGeogCSFrom( new_handle, _handle );
        if ( err == OGRERR_NONE )
        {
            const_cast<SpatialReference*>(this)->_geo_srs = new SpatialReference( new_handle );
        }
        else
        {
            OSRDestroySpatialReference( new_handle );
        }
    }

    return _geo_srs.get();
}

bool
SpatialReference::isMercator() const
{
    if ( !_initialized )
        const_cast<SpatialReference*>(this)->init();
    return _is_mercator;
}

bool 
SpatialReference::isNorthPolar() const
{
    if ( !_initialized )
        const_cast<SpatialReference*>(this)->init();
    return _is_north_polar;
}

bool 
SpatialReference::isSouthPolar() const
{
    if ( !_initialized )
        const_cast<SpatialReference*>(this)->init();
    return _is_south_polar;
}

osg::CoordinateSystemNode*
SpatialReference::createCoordinateSystemNode() const
{
    osg::CoordinateSystemNode* csn = new osg::CoordinateSystemNode();

    if ( !_initialized )
        const_cast<SpatialReference*>(this)->init();

    if ( !_wkt.empty() )
    {
        csn->setFormat( "WKT" );
        csn->setCoordinateSystem( _wkt );
    }
    else if ( !_proj4.empty() )
    {
        csn->setFormat( "PROJ4" );
        csn->setCoordinateSystem( _proj4 );
    }
    else
    {
        csn->setFormat( _init_type );
        csn->setCoordinateSystem( _init_str );
    }
    
    csn->setEllipsoidModel( _ellipsoid.get() );
    
    return csn;
}

// Make a MatrixTransform suitable for use with a Locator object based on the given extents.
// Calling Locator::setTransformAsExtents doesn't work with OSG 2.6 due to the fact that the
// _inverse member isn't updated properly.  Calling Locator::setTransform works correctly.
static osg::Matrixd
getTransformFromExtents(double minX, double minY, double maxX, double maxY)
{
    osg::Matrixd transform;
    transform.set(
        maxX-minX, 0.0,       0.0, 0.0,
        0.0,       maxY-minY, 0.0, 0.0,
        0.0,       0.0,       1.0, 0.0,
        minX,      minY,      0.0, 1.0); 
    return transform;
}

osgTerrain::Locator*
SpatialReference::createLocator(double xmin, double ymin, double xmax, double ymax,
                                bool plate_carre ) const
{
    osgTerrain::Locator* locator = new osgTerrain::Locator();
    locator->setEllipsoidModel( (osg::EllipsoidModel*)getEllipsoid() );
    locator->setCoordinateSystemType( isGeographic()? osgTerrain::Locator::GEOGRAPHIC : osgTerrain::Locator::PROJECTED );
    // note: not setting the format/cs on purpose.

    if ( isGeographic() && !plate_carre )
    {
        locator->setTransform( getTransformFromExtents(
            osg::DegreesToRadians( xmin ),
            osg::DegreesToRadians( ymin ),
            osg::DegreesToRadians( xmax ),
            osg::DegreesToRadians( ymax ) ) );
    }
    else
    {
        locator->setTransform( getTransformFromExtents( xmin, ymin, xmax, ymax ) );
    }
    return locator;
}

bool
SpatialReference::transform( double x, double y, const SpatialReference* out_srs, double& out_x, double& out_y ) const
{        
    //Check for equivalence and return if the coordinate systems are the same.
    if (isEquivalentTo(out_srs))
    {
        out_x = x;
        out_y = y;
        return true;
    }

    GDAL_SCOPED_LOCK;

    preTransform(x, y);

    void* xform_handle = NULL;
    TransformHandleCache::const_iterator itr = _transformHandleCache.find(out_srs->getWKT());
    if (itr != _transformHandleCache.end())
    {
        osg::notify(osg::DEBUG_INFO) << "[osgEarth::SpatialReference] using cached transform handle" << std::endl;
        xform_handle = itr->second;
    }
    else
    {
        xform_handle = OCTNewCoordinateTransformation( _handle, out_srs->_handle);
        const_cast<SpatialReference*>(this)->_transformHandleCache[out_srs->getWKT()] = xform_handle;
    }

    if ( !xform_handle )
    {
        osg::notify( osg::WARN )
            << "[osgEarth::SRS] SRS xform not possible" << std::endl
            << "    From => " << getName() << std::endl
            << "    To   => " << out_srs->getName() << std::endl;
        return false;
    }

    double temp_x = x;
    double temp_y = y;
    double temp_z = 0.0;
    bool result;

    if ( OCTTransform( xform_handle, 1, &temp_x, &temp_y, &temp_z ) )
    {
        result = true;
        out_x = temp_x;
        out_y = temp_y;

        out_srs->postTransform(out_x, out_y);
    }
    else
    {
        osg::notify( osg::WARN ) << "[osgEarth::SRS] Failed to xform a point from "
            << getName() << " to " << out_srs->getName()
            << std::endl;
        result = false;
    }
    return result;
}

bool
SpatialReference::transformPoints(const SpatialReference* out_srs,
                                  double* x, double* y,
                                  unsigned int numPoints,
                                  bool ignore_errors ) const
{
    //Check for equivalence and return if the coordinate systems are the same.
    if (isEquivalentTo(out_srs)) return true;
    
    GDAL_SCOPED_LOCK;

    for (unsigned int i = 0; i < numPoints; ++i)
    {
        preTransform(x[i], y[i]);
    }

    void* xform_handle = NULL;
    TransformHandleCache::const_iterator itr = _transformHandleCache.find(out_srs->getWKT());
    if (itr != _transformHandleCache.end())
    {
        osg::notify(osg::DEBUG_INFO) << "[osgEarth::SRS] using cached transform handle" << std::endl;
        xform_handle = itr->second;
    }
    else
    {
        xform_handle = OCTNewCoordinateTransformation( _handle, out_srs->_handle);
        const_cast<SpatialReference*>(this)->_transformHandleCache[out_srs->getWKT()] = xform_handle;
    }

    if ( !xform_handle )
    {
        osg::notify( osg::WARN )
            << "[osgEarth::SRS] SRS xform not possible" << std::endl
            << "    From => " << getName() << std::endl
            << "    To   => " << out_srs->getName() << std::endl;
        return false;
        }

    double *temp_z = new double[numPoints];
    bool success;

    success = OCTTransform( xform_handle, numPoints, x, y, temp_z ) > 0;

    if ( success || ignore_errors )
    {
        for (unsigned int i = 0; i < numPoints; ++i)
        {
            out_srs->postTransform(x[i], y[i]);
        }
    }
    else
    {
        osg::notify( osg::WARN ) << "[osgEarth::SRS] Failed to xform a point from "
            << getName() << " to " << out_srs->getName()
            << std::endl;
    }
    delete[] temp_z;
    return success;
}

bool
SpatialReference::transformExtent(const SpatialReference* to_srs,
                                  double& in_out_xmin,
                                  double& in_out_ymin,
                                  double& in_out_xmax,
                                  double& in_out_ymax) const
{
    double x[2] = { in_out_xmin, in_out_xmax };
    double y[2] = { in_out_ymin, in_out_ymax };
    bool ok = transformPoints( to_srs, x, y, 2 );
    if ( ok )
    {
        in_out_xmin = x[0];
        in_out_ymin = y[0];
        in_out_xmax = x[1];
        in_out_ymax = y[1];
    }
    return ok;
}

void
SpatialReference::init()
{
    GDAL_SCOPED_LOCK;
    
    _is_geographic = OSRIsGeographic( _handle ) != 0;
    
    int err;
    double semi_major_axis = OSRGetSemiMajor( _handle, &err );
    double semi_minor_axis = OSRGetSemiMinor( _handle, &err );
    // there's a problem in one or more of the OSG manipulators that causes intersection errors
    // if the ellipsoid model is a perfect sphere. This "fudge factor" works around that for now
    // without affecting the math too much.
    //double fudge_factor = semi_major_axis == semi_minor_axis? 0.0001 : 0.0;
    //_ellipsoid = new osg::EllipsoidModel( semi_major_axis + fudge_factor, semi_minor_axis );
    _ellipsoid = new osg::EllipsoidModel( semi_major_axis, semi_minor_axis );

    if ( _name.empty() || _name == "unnamed" )
    {
        _name = _is_geographic? 
            getOGRAttrValue( _handle, "GEOGCS", 0 ) : 
            getOGRAttrValue( _handle, "PROJCS", 0 );
    }

    std::string proj = getOGRAttrValue( _handle, "PROJECTION", 0, true );
    _is_mercator = !proj.empty() && proj.find("mercator")==0;

    if ( !proj.empty() && proj.find("polar_stereographic") != std::string::npos )
    {
        double lat = as<double>( getOGRAttrValue( _handle, "latitude_of_origin", 0, true ), -90.0 );
        _is_north_polar = lat > 0.0;
        _is_south_polar = lat < 0.0;
    }
	else
	{
		_is_north_polar = false;
		_is_south_polar = false;
	}

    if ( _name == "unnamed" )
    {
        _name =
            _is_geographic? "Geographic CS" :
            _is_mercator? "Mercator CS" :
            ( !proj.empty()? proj : "Projected CS" );
    }


    char* wktbuf;
    if ( OSRExportToWkt( _handle, &wktbuf ) == OGRERR_NONE )
    {
        _wkt = wktbuf;
        OGRFree( wktbuf );
    }
    
    char* proj4buf;
    if ( OSRExportToProj4( _handle, &proj4buf ) == OGRERR_NONE )
    {
        _proj4 = proj4buf;
        OGRFree( proj4buf );
    }

    _initialized = true;
}
