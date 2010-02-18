/* 
 * File:   Mica2Mote.h
 * Author: prakash
 *
 * Created on February 8, 2010, 1:17 PM
 */

#ifndef _MICA2MOTE_H
#define	_MICA2MOTE_H

#include "spatialcomputer.h"

/*****************************************************************************
 *  TESTBED MOTE IO                                                          *
 *****************************************************************************/
class MoteIO : public Layer, public HardwarePatch {
 public:
  MoteIO(Args* args, SpatialComputer* parent);
  void add_device(Device* d);
  BOOL handle_key(KeyEvent* event);
  void dump_header(FILE* out); // list log-file fields
  // hardware emulation
  void set_speak (NUM_VAL period);
  NUM_VAL read_light_sensor(VOID);
  NUM_VAL read_microphone (VOID);
  NUM_VAL read_temp (VOID);
  NUM_VAL read_short (VOID);       // test for conductivity
  NUM_VAL read_button (uint8_t n);
  NUM_VAL read_slider (uint8_t ikey, uint8_t dkey, NUM_VAL init, // frob knob
		       NUM_VAL incr, NUM_VAL min, NUM_VAL max);
};

class DeviceMoteIO : public DeviceLayer {
  MoteIO* parent;
 public:
  BOOL button;
  DeviceMoteIO(MoteIO* parent, Device* d) : DeviceLayer(d)
    { this->parent=parent; button=FALSE; }
  void visualize(Device* d);
  BOOL handle_key(KeyEvent* event);
  void copy_state(DeviceLayer* src) {} // to be called during cloning
  void dump_state(FILE* out, int verbosity); // print state to file
};

#endif	/* _MICA2MOTE_H */

