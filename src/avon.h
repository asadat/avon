/*
  File: avon.c
  Description: a lightweight HTTP server for robots and robot simulators
  Author: Richard Vaughan (vaughan@sfu.ca)
  Date: 1 November 2010
  Version: $Id:$  
  License: LGPL v3.
 */

#include <time.h> // for time_t
#include <stdint.h> // for uint16_t

#define AV_DEFAULT_PORT 8000

typedef enum
	{
		AV_INTERFACE_SIM = 0,
		AV_INTERFACE_GENERIC,
		//AV_INTERFACE_POSITION2D,
		AV_INTERFACE_RANGER,
		AV_INTERFACE_FIDUCIAL,
		AV_INTERFACE_COUNT // must be the last entry
	} av_interface_t;

/** Pose in parent's CS, velocity and acceleration in local CS, all in
	 6 axes. */
typedef struct
{
  uint64_t time;
  double p[6], v[6], a[6];
} av_pva_t;

/** Transformation into local object coordinates, and object size in
	 local coordinates. */
typedef struct
{
  uint64_t time;
  double pose[6]; ///< 6dof pose
  double extent[3]; ///< 3d size of bounding box in local coord frame
} av_geom_t;


/** specify the bounds of a value */
typedef struct
{
	double min, max;
} av_bounds_t;

// - POSITION2D --------------------------------------------

/*  hmm, given PVA support for every model, maybe it's not necessary to */
/*  not provide a special p2d model type? Just don't support position */
/* control directly then there's nothing to do here. think about this
	 more? */

/* typedef struct */
/* { */
/* 	uint64_t time; */

/* 	enum  */
/* 		{ */
/* 			AV_POSITION2D_CMD_MODE_SPEED = 0, */
/* 			AV_POSITION2D_CMD_MODE_POSITION */
/* 		} mode; */
	
/* 	/\** [0]=x, [1]=y, [2]=yaw, pose or speed, depending on the mode. *\/ */
/* 	double value[3];  */

/* } av_position2d_cmd_t; */

/* typedef struct */
/* { */
/* 	uint64_t time; */

/* 	enum  */
/* 		{ */
/* 			AV_POSITION2D_CMD_MODE_SPEED = 0, */
/* 			AV_POSITION2D_CMD_MODE_POSITION */
/* 		} mode; */
	
/* 	/\** [0]=x, [1]=y, [2]=yaw, pose or speed, depending on the mode. *\/ */
/* 	double goal_value[3]; */
/* 	/\** [0]=x, [1]=y, [2]=yaw, pose or speed, depending on the mode. *\/ */
/* 	double actual_value[3]; */

/* 	bool stall; */
/* } av_position2d_data_t */

//- FIDUCIAL -----------------------------------------------

#define AV_FIDUCIALS_DETECTED_MAX 1024

typedef struct
{
	/** pose of the fiducial in spherical coordinates, where [0] is the
			bearing in radians, [1] is the azimuth in radians, [2] is the range
			in meters. */
	double pose[3];
	/** pose and size of the fiducial in its own local coordinate
			system, with origin at [pose] */
	av_geom_t geom;
	uint64_t id;
} av_fiducial_t;

typedef struct
{
	uint64_t time;
	uint32_t fiducial_count;
	av_fiducial_t fiducials[AV_FIDUCIALS_DETECTED_MAX];
} av_fiducial_data_t;

typedef struct
{
	/** the bounds of the field-of-view [0] is bearing, [1] is azimuth, [3] is range */
	 av_bounds_t fov[3];	 
} av_fiducial_cfg_t;

//- RANGER --------------------------------------------------

#define AV_RANGER_TRANSDUCERS_MAX 64
#define AV_RANGER_SAMPLES_MAX 1024

enum
  {
	 AV_SAMPLE_BEARING=0,
	 AV_SAMPLE_AZIMUTH=1,
	 AV_SAMPLE_RANGE=2,
	 AV_SAMPLE_INTENSITY=3
  };

typedef struct
{ 
  uint64_t time;
  
  /** origin of the ranger beams in local coordinates (x,y,z,r,p,a) */
  double pose[6];
  
  /** number of samples to follow */
  uint32_t sample_count;  
  /** BARI: ranger samples in spherical coordinates, specifying a
		point in space where the range beam terminated, in spherical
		coordinates. [0] is the bearing in radians (angle around z - the
		horizontal angle), [1] is azimuth in radians (angle around y -
		the vertical angle), [2] is range (distance along beam in
		meters), [3] is intensity, where 0 means no reflection was
		detected (e.g. laser beam timeout, or IR reflection below
		minimum threshold). Use BARI to help remember the order, or use
		the enum above to extract. */
  double samples[AV_RANGER_SAMPLES_MAX][4]; 
} av_ranger_transducer_data_t;

typedef struct
{
	uint64_t time;
  /** number of transduders to follow */
  uint32_t transducer_count;  
  /** transducer array */
  av_ranger_transducer_data_t transducers[AV_RANGER_TRANSDUCERS_MAX]; 
} av_ranger_data_t;


typedef struct
{
  uint64_t time;
  uint32_t transducer_count;
  
  struct
  {
	 /** the pose and size of the transducer object */
	 av_geom_t geom;
	 /** the bounds of the field-of-view [0] is bearing, [1] is azimuth, [3] is range */
	 av_bounds_t fov[3];	 
  } transducers[AV_RANGER_TRANSDUCERS_MAX];
  
} av_ranger_cfg_t;

//---------------------------------------------------------

typedef struct
{
  uint64_t time;
  av_interface_t interface;
  const void* data;
  size_t len;
} av_msg_t;

typedef int (*av_pva_set_t)( void* obj, av_pva_t* pva );
typedef int (*av_pva_get_t)( void* obj, av_pva_t* pva );
typedef int (*av_geom_set_t)( void* obj, av_geom_t* geom );
typedef int (*av_geom_get_t)( void* obj, av_geom_t* geom );

typedef int (*av_data_get_t)( void* obj, av_msg_t* data );
typedef int (*av_cmd_set_t)( void* obj, av_msg_t* cmd );
typedef int (*av_cfg_set_t)( void* obj, av_msg_t* cfg );
typedef int (*av_cfg_get_t)( void* obj, av_msg_t* cfg );

typedef uint64_t (*av_clock_get_t)(void* obj);

int av_init( const char* hostname, 
				 const uint16_t port, 
				 const char* rootdir, 
				 const int verbose,
				 const char* backend_name,
				 const char* backend_version );

/** Frees resources. */
void av_fini( void );

void av_startup( void );

/** Handle server events. Blocks until at least one event occurs. */
void av_wait( void );

/** Handle server events. Returns immediately if none are pending. */
void av_check();

int av_register_model( const char* name, 
											 const char* prototype,
											 av_interface_t interface, 
											 const char* parent, 
											 void* handle );

int av_install_clock_callbacks( av_clock_get_t clock_get, void* obj );

int av_install_generic_callbacks( av_pva_set_t pva_set,
																	av_pva_get_t pva_get, 
																	av_geom_set_t geom_set, 
																	av_geom_get_t geom_get );

int av_install_interface_callbacks( av_interface_t interface,
																		av_data_get_t data_get,
																		av_cmd_set_t cmd_set,
																		av_cfg_set_t cfg_set,
																		av_cfg_get_t cfg_get );

