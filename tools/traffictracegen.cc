/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*  Author: Sergio Herreria, Sergio.Herreria@det.uvigo.es */
/*  Date: Jan 2009 */
/*  Copyright (C) Sergio Herreria 2009. All rights reserved. */

#include "trafgen.h"

class TrafficTraceGen : public TrafficGenerator {
public:
  TrafficTraceGen() : tfile_(0) {};
  int command(int argc, const char*const* argv);
  virtual double next_interval(int &);
protected:
  virtual void start();
  void timeout();
private:
  FILE *tfile_;
};

static class TrafficTraceGenClass : public TclClass {
public:
  TrafficTraceGenClass() : TclClass("Application/Traffic/TraceGen") {}
  TclObject* create(int, const char*const*) {
    return(new TrafficTraceGen());
  }
} class_traffictracegen;

int TrafficTraceGen::command(int argc, const char*const* argv)
{
  if (argc == 3) {
    if (strcmp(argv[1], "attach-tracefile") == 0) {
      if ((tfile_ = fopen(argv[2], "r")) == NULL) {
	printf("can't open file %s\n", argv[2]);
	return(TCL_ERROR);
      }
      return(TCL_OK);
    }
  }
  return (TrafficGenerator::command(argc, argv));
}

void TrafficTraceGen::start()
{
  running_ = 1;
  timeout();
}

void TrafficTraceGen::timeout()
{
  if (! running_)
    return;

  agent_->sendmsg(size_);
  /* figure out when to send the next one */
  nextPkttime_ = next_interval(size_);
  /* schedule it */
  if (nextPkttime_ >= 0)
    timer_.resched(nextPkttime_);
  else
    fclose(tfile_);
}

double TrafficTraceGen::next_interval(int& size)
{
  double interval;
  if (fscanf(tfile_,"%lf %d",&interval,&size) == EOF)
    return -1;
  return(interval);
}
