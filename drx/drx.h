/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Author: Sergio Herreria, Sergio.Herreria@det.uvigo.es 
 * Date: Jun 2014 
 * Copyright (C) Sergio Herreria 2014. All rights reserved.
 */

#ifndef ns_drx_h
#define ns_drx_h

#include <math.h>
#include "queue.h"
#include "config.h"
#include "timer-handler.h"
#include "delay.h"

/*
 * DRX timer
 */

class DRX;

class DRXTimer : public TimerHandler {
public: 
	DRXTimer(DRX *drx_q) : TimerHandler() { drx_q_ = drx_q; }
protected:
	virtual void expire(Event *);
	DRX *drx_q_;
};

/*
 * DRX states
 */

enum TracedState {
	ACTIVE_IDLE, 
	ACTIVE, 
	DRX_ACTIVE, 
	DRX_SHORT_INACTIVE, 
	DRX_LONG_INACTIVE, 
	DRX_SHORT_TO_ACTIVE, 
	DRX_LONG_TO_ACTIVE
};

/*
 * DRX queue
 */

class DRX : public Queue {
public:
	DRX(): timer_(this) { 
		q_ = new PacketQueue;
		bind_bool("drx_enabled_", &drx_enabled_);
		// DRX parameters
		bind("subframe_length_", &subframe_length_);
		bind("inactivity_timer_", &inactivity_timer_);
		bind("short_cycle_length_", &short_cycle_length_);
		bind("short_cycle_timer_", &short_cycle_timer_);
		bind("long_cycle_length_", &long_cycle_length_);
		bind("on_duration_timer_", &on_duration_timer_);
		bind("short_to_active_length_", &short_to_active_length_);
		bind("long_to_active_length_", &long_to_active_length_);
		// Coalescing parameters
		bind("queue_threshold_", &queue_threshold_);
		bind("time_threshold_", &time_threshold_);
		bind("target_avg_delay_", &target_avg_delay_);
		// DRX variables
		channel_ = 0;
		state_ = ACTIVE;
		consecutive_cycles_ = 0;
		hol_packet_arrival_time_ = 0.;
		dynamic_queue_threshold_ = 0;
	}
	~DRX() {
		delete q_;
	}
	void trace(TracedState);
	TracedState new_drx_state();
	
protected:
	int command(int argc, const char*const* argv); 

	void reset();
	void enque(Packet*);
	Packet* deque();

	int disable_drx();               // (bool) should be DRX mode disabled?
	
	PacketQueue *q_;	         // underlying FIFO queue

	Tcl_Channel channel_;            // output TCL channel
	DRXTimer timer_;                 // DRX timer

	int drx_enabled_;                // true if DRX can be enabled (bool)
	double subframe_length_;         // physical subframe length (seconds)
	int inactivity_timer_;           // time to wait before enabling DRX (subframes)
	int short_cycle_length_;         // length of short DRX cycles (subframes)
	int short_cycle_timer_;          // number of initial short DRX cycles before transitioning to long DRX cycles
	int long_cycle_length_;          // length of long DRX cycles (subframes)
	int on_duration_timer_;          // time with reception enabled in each DRX cycle (subframes)
	int short_to_active_length_;     // time required to enable reception after a short DRX cycle (subframes)
	int long_to_active_length_;      // time required to enable reception after a long DRX cycle (subframes)

	double queue_threshold_;         // minimum number of queued packets required to disable DRX (pkts)
	double time_threshold_;          // maximum waiting time for queued packets (seconds)
	int dynamic_queue_threshold_;    // true if the queue threshold is adjusted dynamically (bool)
	int cycle_packets_sent_;
	double cycle_sum_delay_;
	double target_avg_delay_;
	double avg_interarrival_time_;
	double prev_arrival_time_;

	TracedState state_;              // queue state
	int consecutive_cycles_;         // current number of consecutive cycles in DRX state
	double hol_packet_arrival_time_; // arrival time of the first packet in current DRX cycle
};

#endif
