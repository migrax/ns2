/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*  Author: Sergio Herreria, Sergio.Herreria@det.uvigo.es */
/*  Date: Oct 2011 */
/*  Copyright (C) Sergio Herreria 2011. All rights reserved. */

#ifndef ns_green_tail_h
#define ns_green_tail_h

#include <string.h>
#include <math.h>
#include "queue.h"
#include "config.h"
#include "delay.h"
#include "timer-handler.h"

/*
 * Sleep timer 
 */

class GreenTail;

class SleepTimer : public TimerHandler {
public: 
	SleepTimer(GreenTail *gq) : TimerHandler() { gq_ = gq; }
protected:
	virtual void expire(Event *);
	GreenTail *gq_;
};

/*
 * A bounded, green-tail queue
 */

enum TracedEvent {ON, OFF, TRA, TRAOFF};

class GreenTail : public Queue {
public:
	GreenTail(): st_(this) { 
		q_ = new PacketQueue; 
		pq_ = q_;
		bind_bool("drop_front_", &drop_front_);
		bind_bool("summarystats_", &summarystats);
		bind_bool("queue_in_bytes_", &qib_);  // boolean: q in bytes?
		bind("mean_pktsize_", &mean_pktsize_);
		bind("mode_", &mode_);
		bind("transition_time_", &transition_time_);
		bind("sleep_transition_time_", &sleep_transition_time_);
		bind("max_sleep_time_", &max_sleep_time_);
		bind("wake_q_th_", &wake_q_th_);
		channel_ = 0;
	}
	TracedEvent state() { return state_; }
	int transition_queue();
	void trace(TracedEvent);
	~GreenTail() {
		delete q_;
	}
	
protected:
	void reset();
	int command(int argc, const char*const* argv); 
	void enque(Packet*);
	Packet* deque();
	void shrink_queue();	/* to shrink queue and drop excessive packets */
	double sleep_queue();

	PacketQueue *q_;	/* underlying FIFO queue */
	int drop_front_;	/* drop-from-front (rather than from tail) */
	int summarystats;
	void print_summarystats();
	int qib_;       	/* bool: queue measured in bytes? */
	int mean_pktsize_;	/* configured mean packet size in bytes */
	double bandwidth_;

	Tcl_Channel channel_;
	TracedEvent state_;
	int mode_;

	SleepTimer st_;
	double sleep_time_;
	double transition_time_;
	double sleep_transition_time_;

	double init_sleep_time_;
	double max_sleep_time_;
	int wake_q_th_;
};

#endif
