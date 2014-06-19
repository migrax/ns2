/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Author: Sergio Herreria, Sergio.Herreria@det.uvigo.es 
 * Date: Jun 2014 
 * Copyright (C) Sergio Herreria 2014. All rights reserved.
 */

#include "drx.h"

static class DRXClass : public TclClass {
public:
	DRXClass() : TclClass("Queue/DRX") {}
	TclObject* create(int, const char*const*) {
		return (new DRX);
	}
} class_drx;

void DRX::reset()
{
	trace(ACTIVE_IDLE);
	if (queue_threshold_ == 0) {
		queue_threshold_ = 1.0;
		dynamic_queue_threshold_ = 1;
		cycle_packets_sent_ = 0;
		cycle_sum_delay_ = prev_arrival_time_ = avg_interarrival_time_ = 0.0;
	}
	Queue::reset();
}

int DRX::command(int argc, const char*const* argv) 
{
	if (argc == 3) {
		if (strcmp(argv[1], "trace") == 0) {
			int aux;
			const char* id = argv[2];
			channel_ = Tcl_GetChannel(Tcl::instance().interp(), (char *)id, &aux);
			if (channel_ == 0) {
				Tcl::instance().resultf("trace: can't attach %s for writing", id);
				return (TCL_ERROR);
			}
			return (TCL_OK);
		}
		if (!strcmp(argv[1], "packetqueue-attach")) {
			delete q_;
			if (!(q_ = (PacketQueue *) TclObject::lookup(argv[2]))) {
				return (TCL_ERROR);
			} else {
				pq_ = q_;
				return (TCL_OK);
			}
		}
	}
	return Queue::command(argc, argv);
}

void DRX::enque(Packet* p)
{
	if (p == 0) {
		return;
	}
	
	if (q_->length() + 1 >= qlim_) {
		drop(p);
	} else {
		double now = Scheduler::instance().clock();
		if (dynamic_queue_threshold_) {
			HDR_CMN(p)->timestamp() = now;
			avg_interarrival_time_ = avg_interarrival_time_ ? 
				0.125 * (now - prev_arrival_time_) + 0.875 * avg_interarrival_time_ : 
				now - prev_arrival_time_;
			prev_arrival_time_ = now;
		}
		if (state_ != ACTIVE && state_ != ACTIVE_IDLE && hol_packet_arrival_time_ == 0) {
			hol_packet_arrival_time_ = now;
		}
		q_->enque(p);
	}
}

Packet* DRX::deque()
{
	Packet *p;
	double now = Scheduler::instance().clock();

	switch (state_) {
	case ACTIVE:
		p = q_->deque();
		if (p != 0 && q_->length() == 0) {
			timer_.resched(subframe_length_);			
		}
		break;
	case ACTIVE_IDLE:
		p = 0;
		if (q_->length() > 0) {
			timer_.resched(subframe_length_ * (ceil(now / subframe_length_) - now / subframe_length_));
		}
	case DRX_ACTIVE:
		p = 0;
		if (q_->length() >= queue_threshold_) {
			state_ = ACTIVE_IDLE;
			timer_.resched(subframe_length_ * (ceil(now / subframe_length_) - now / subframe_length_));
		}
	case DRX_SHORT_INACTIVE: case DRX_LONG_INACTIVE: case DRX_SHORT_TO_ACTIVE: case DRX_LONG_TO_ACTIVE:
		p = 0;
	}

	if (p != 0 && dynamic_queue_threshold_) {
		cycle_packets_sent_++;
		cycle_sum_delay_ += now - HDR_CMN(p)->timestamp();
	}

	return p;
}

void DRX::trace(TracedState state)
{
	if (state_ == state) {
		return;
	}
	state_ = state;

	if (channel_) {
		char traced_state_line[64];
		char * traced_state_strings[] = {
			"ACTIVE_IDLE", 
			"ACTIVE", 
			"DRX_ACTIVE", 
			"DRX_SHORT_INACTIVE", 
			"DRX_LONG_INACTIVE", 
			"DRX_SHORT_TO_ACTIVE", 
			"DRX_LONG_TO_ACTIVE"
		};
		snprintf(traced_state_line, 64, "%.9f %s %d %f\n", Scheduler::instance().clock(), traced_state_strings[state_], q_->length(), queue_threshold_);
		(void)Tcl_Write(channel_, traced_state_line, -1);
	}       

	switch (state_) {
	case ACTIVE:
		consecutive_cycles_ = 0;
		hol_packet_arrival_time_ = 0;
		recv(0,0);
		break;
	case ACTIVE_IDLE:
		timer_.resched(subframe_length_ * inactivity_timer_);
		break;
	case DRX_ACTIVE:
		timer_.resched(subframe_length_ * on_duration_timer_);
		break;
	case DRX_SHORT_INACTIVE:
		consecutive_cycles_++;
		timer_.resched(subframe_length_ * (short_cycle_length_ - on_duration_timer_ - short_to_active_length_));
		if (dynamic_queue_threshold_ && cycle_packets_sent_) {
			double cycle_avg_delay = cycle_sum_delay_ / cycle_packets_sent_;
			double diff_threshold = 2.0 * (target_avg_delay_ - cycle_avg_delay) / avg_interarrival_time_;
			if (queue_threshold_ >= 2.0) {
				diff_threshold /= 1.0 - 2.0/queue_threshold_/queue_threshold_;
			}
			queue_threshold_ += diff_threshold;
			if (queue_threshold_ < 1) {
				queue_threshold_ = 1;
			}
			cycle_packets_sent_ = 0;
			cycle_sum_delay_ = 0.0;
		}
		break;
	case DRX_LONG_INACTIVE:
		consecutive_cycles_++;
		timer_.resched(subframe_length_ * (long_cycle_length_ - on_duration_timer_ - long_to_active_length_));
		break;
	case DRX_SHORT_TO_ACTIVE:
		timer_.resched(subframe_length_ * short_to_active_length_);
		break;
	case DRX_LONG_TO_ACTIVE:
		timer_.resched(subframe_length_ * long_to_active_length_);
		break;
	}
}

/*
 * DRX timer
 */

int DRX::disable_drx()
{
	int disable_drx = 0;

	if (q_->length() >= queue_threshold_) {
		disable_drx = 1;
	} else if (hol_packet_arrival_time_ > 0) {
		int cycle_length = consecutive_cycles_ < short_cycle_timer_ ? short_cycle_length_ : long_cycle_length_;
		if (Scheduler::instance().clock() - hol_packet_arrival_time_ + subframe_length_ * cycle_length >= time_threshold_) {
			disable_drx = 1;
		}
	}
	return disable_drx;
}

TracedState DRX::new_drx_state()
{
	TracedState new_state;
	switch (state_) {
	case ACTIVE:
		new_state = q_->length() > 0 ? ACTIVE : ACTIVE_IDLE;
		break;
	case ACTIVE_IDLE:
		if (q_->length() > 0) {
			new_state = ACTIVE;
		} else if (drx_enabled_) {
			new_state = DRX_SHORT_INACTIVE;
		} else {
			new_state = ACTIVE_IDLE;
		}
		break;
	case DRX_ACTIVE:
		new_state = consecutive_cycles_ >= short_cycle_timer_ ? DRX_LONG_INACTIVE : DRX_SHORT_INACTIVE;
		break;
	case DRX_SHORT_INACTIVE:
		new_state = DRX_SHORT_TO_ACTIVE;
		break;
	case DRX_LONG_INACTIVE:
		new_state = DRX_LONG_TO_ACTIVE;
		break;
	case DRX_SHORT_TO_ACTIVE: case DRX_LONG_TO_ACTIVE:
		new_state = disable_drx() ? ACTIVE : DRX_ACTIVE;
		break;
	}
	return new_state;
}

void DRXTimer::expire(Event*)
{
	TracedState new_state = drx_q_->new_drx_state();
	drx_q_->trace(new_state);
}
