/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*  Author: Sergio Herreria, Sergio.Herreria@det.uvigo.es */
/*  Date: Oct 2011 */
/*  Copyright (C) Sergio Herreria 2011. All rights reserved. */

#include "green-tail.h"

static class GreenTailClass : public TclClass {
public:
	GreenTailClass() : TclClass("Queue/GreenTail") {}
	TclObject* create(int, const char*const*) {
		return (new GreenTail);
	}
} class_green_tail;

/*
 * Green-tail
 */

void GreenTail::reset()
{
	init_sleep_time_ = -1.;
	trace(ON);
	Queue::reset();
}

int 
GreenTail::command(int argc, const char*const* argv) 
{
	if (argc==2) {
		if (strcmp(argv[1], "printstats") == 0) {
			print_summarystats();
			return (TCL_OK);
		}
 		if (strcmp(argv[1], "shrink-queue") == 0) {
 			shrink_queue();
 			return (TCL_OK);
 		}
	}
	if (argc == 3) {
		if (strcmp(argv[1], "trace") == 0) {
			int aux;
			const char* id = argv[2];
			channel_ = Tcl_GetChannel(Tcl::instance().interp(), (char*)id, &aux);
			if (channel_ == 0) {
				Tcl::instance().resultf("trace: can't attach %s for writing", id);
				return (TCL_ERROR);
			}
			return (TCL_OK);
		}
		if (!strcmp(argv[1], "packetqueue-attach")) {
			delete q_;
			if (!(q_ = (PacketQueue*) TclObject::lookup(argv[2])))
				return (TCL_ERROR);
			else {
				pq_ = q_;
				return (TCL_OK);
			}
		}
	}
	return Queue::command(argc, argv);
}

void GreenTail::enque(Packet* p)
{
	if (p == 0) return;

	if (summarystats) {
                Queue::updateStats(qib_?q_->byteLength():q_->length());
	}

	int qlimBytes = qlim_ * mean_pktsize_;
	if ((!qib_ && (q_->length() + 1) >= qlim_) ||
  	(qib_ && (q_->byteLength() + hdr_cmn::access(p)->size()) >= qlimBytes)){
		// if the queue would overflow if we added this packet...
		if (drop_front_) { /* remove from head of queue */
			q_->enque(p);
			Packet *pp = q_->deque();
			drop(pp);
		} else {
			drop(p);
		}
	} else {
		q_->enque(p);
	}

	if (state_ == OFF && transition_queue())
		trace(TRA);
}

void GreenTail::shrink_queue() 
{
        int qlimBytes = qlim_ * mean_pktsize_;
	if (debug_)
		printf("shrink-queue: time %5.2f qlen %d, qlim %d\n",
		       Scheduler::instance().clock(),
		       q_->length(), qlim_);
        while ((!qib_ && q_->length() > qlim_) || 
            (qib_ && q_->byteLength() > qlimBytes)) {
                if (drop_front_) { /* remove from head of queue */
                        Packet *pp = q_->deque();
                        drop(pp);
                } else {
                        Packet *pp = q_->tail();
                        q_->remove(pp);
                        drop(pp);
                }
        }
}

Packet* GreenTail::deque()
{
        if (summarystats && &Scheduler::instance() != NULL) {
                Queue::updateStats(qib_?q_->byteLength():q_->length());
        }

	if (mode_ > 0) {
		if ((state_ != ON) || (sleep_queue() > 0)) 
			return 0;
	}
	return q_->deque();
}

void GreenTail::print_summarystats()
{
        printf("True average queue: %5.3f", true_ave_);
        if (qib_)
                printf(" (in bytes)");
        printf(" time: %5.3f\n", total_time_);
}

double GreenTail::sleep_queue()
{
	sleep_time_ = 0;

	if (mode_ == 1) {
		/* Frame transmission */
		if (q_->length() == 0) sleep_time_ = max_sleep_time_;
	} else if (mode_ == 2) {
		/* Burst transmission */
		if (q_->length() == 0) sleep_time_ = max_sleep_time_ + 0.000000001;
	}

	if (sleep_time_ > 0)
		trace(TRAOFF);		

	return sleep_time_;
}

int GreenTail::transition_queue()
{
	if (state_ == OFF && mode_ == 2  && init_sleep_time_ == -1 && q_->length() > 0) {
		init_sleep_time_ = Scheduler::instance().clock();
		st_.resched(sleep_time_);
	}
	double acc_sleep_time = init_sleep_time_ > 0 ? Scheduler::instance().clock() - init_sleep_time_ : 0.;

	if (((mode_ == 1) && (q_->length() > 0)) || // Frame transmission
	    ((mode_ == 2) && (q_->length() >= wake_q_th_)) || // Burst transmission
	    ((mode_ == 2) && (q_->length() > 0) && (acc_sleep_time >= max_sleep_time_))) // Burst transmission
		return 1;

	return 0;
}

void GreenTail::trace(TracedEvent e)
{
	state_ = e;

	if (channel_) {
		char wrk[128];
		double now = Scheduler::instance().clock();
		double acc_sleep_time = init_sleep_time_ > 0 ? now - init_sleep_time_ : 0.;
  		int qlength = qib_ ? q_->byteLength() : q_->length();
		if (e == ON) {
			snprintf(wrk,128,"%.9f ON %d\n", now, qlength);
			init_sleep_time_ = -1.;
		} else if (e == OFF) {
			snprintf(wrk,128,"%.9f OFF %.9f %.9f %d\n", now, sleep_time_, acc_sleep_time, qlength);
			st_.resched(sleep_time_);
		} else if (e == TRA) {
			snprintf(wrk,128,"%.9f TRA %.9f %.9f %d\n", now, transition_time_, acc_sleep_time, qlength);
			st_.resched(transition_time_);
		} else {
			snprintf(wrk,128,"%.9f TRA %.9f %d\n", now, sleep_transition_time_, qlength);
			st_.resched(sleep_transition_time_);
		}
		(void)Tcl_Write(channel_, wrk, -1);
	}
}

/*
 * Sleep timer
 */

void SleepTimer::expire(Event*)
{
	if ((gq_->state() == OFF) || (gq_->state() == TRAOFF)){
		if (gq_->transition_queue())
			gq_->trace(TRA);
		else
			gq_->trace(OFF);
	} else {
		gq_->trace(ON);
		gq_->recv(0,0);
	}
}
