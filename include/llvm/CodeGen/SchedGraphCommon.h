//===-- SchedGraphCommon.h - Scheduling Base Graph ---------------*- C++ -*---=//
//
// A common graph class that is based on the SSA graph. It includes
// extra dependencies that are caused by machine resources.
//
//===-------------------------------------------------------------------------=// 

#ifndef LLVM_CODEGEN_SCHEDGRAPHCOMMON_H
#define LLVM_CODEGEN_SCHEDGRAPHCOMMON_H

#include <iostream>
#include <vector>
#include "llvm/Value.h"


class SchedGraphEdge;
class SchedGraphNode;

/******************** Exported Data Types and Constants ********************/

typedef int ResourceId;
const ResourceId InvalidRID        = -1;
const ResourceId MachineCCRegsRID  = -2; // use +ve numbers for actual regs
const ResourceId MachineIntRegsRID = -3; // use +ve numbers for actual regs
const ResourceId MachineFPRegsRID  = -4; // use +ve numbers for actual regs

//*********************** Public Class Declarations ************************/

class SchedGraphNodeCommon {
protected:
  unsigned ID;
  std::vector<SchedGraphEdge*> inEdges;
  std::vector<SchedGraphEdge*> outEdges;
  int latency;

  friend std::ostream& operator<<(std::ostream& os, const SchedGraphNode& node);
  
public:
  typedef std::vector<SchedGraphEdge*>::      iterator	       iterator;
  typedef std::vector<SchedGraphEdge*>::const_iterator         const_iterator;
  typedef std::vector<SchedGraphEdge*>::      reverse_iterator reverse_iterator;
  typedef std::vector<SchedGraphEdge*>::const_reverse_iterator const_reverse_iterator;
  
  // Accessor methods
  unsigned 		getNodeId	() const { return ID; }
  int			getLatency	() const { return latency; }
  unsigned 		getNumInEdges	() const { return inEdges.size(); }
  unsigned 		getNumOutEdges	() const { return outEdges.size(); }
  

  // Iterators
  iterator		beginInEdges	()	 { return inEdges.begin(); }
  iterator		endInEdges	()	 { return inEdges.end(); }
  iterator		beginOutEdges	()	 { return outEdges.begin(); }
  iterator		endOutEdges	()	 { return outEdges.end(); }
  
  const_iterator	beginInEdges	() const { return inEdges.begin(); }
  const_iterator	endInEdges	() const { return inEdges.end(); }
  const_iterator	beginOutEdges	() const { return outEdges.begin(); }
  const_iterator	endOutEdges	() const { return outEdges.end(); }


  // Debugging support
  friend std::ostream& operator<<(std::ostream& os, const SchedGraphNodeCommon& node);

  void		dump	(int indent=0) const;

protected:
  friend class SchedGraph;		
  friend class SchedGraphCommon;
  friend class SchedGraphEdge;		// give access for adding edges
  //friend class ModuloSchedGraph;
  
  void			addInEdge	(SchedGraphEdge* edge);
  void			addOutEdge	(SchedGraphEdge* edge);
  
  void			removeInEdge	(const SchedGraphEdge* edge);
  void			removeOutEdge	(const SchedGraphEdge* edge);
 
  // disable default constructor and provide a ctor for single-block graphs
  SchedGraphNodeCommon();	// DO NOT IMPLEMENT
  
  SchedGraphNodeCommon(unsigned Id);
  
  virtual ~SchedGraphNodeCommon();
};


class SchedGraphEdge {
public:
  enum SchedGraphEdgeDepType {
    CtrlDep, MemoryDep, ValueDep, MachineRegister, MachineResource
  };
  enum DataDepOrderType {
    TrueDep = 0x1, AntiDep=0x2, OutputDep=0x4, NonDataDep=0x8
  };
  
protected:
  SchedGraphNodeCommon*	src;
  SchedGraphNodeCommon*	sink;
  SchedGraphEdgeDepType depType;
  unsigned int          depOrderType;
  int			minDelay; // cached latency (assumes fixed target arch)
  int                   iteDiff;
  
  union {
    const Value* val;
    int          machineRegNum;
    ResourceId   resourceId;
  };
  
public:	
  // For all constructors, if minDelay is unspecified, minDelay is
  // set to _src->getLatency().
  // constructor for CtrlDep or MemoryDep edges, selected by 3rd argument
  /*ctor*/		SchedGraphEdge(SchedGraphNodeCommon* _src,
				       SchedGraphNodeCommon* _sink,
				       SchedGraphEdgeDepType _depType,
				       unsigned int     _depOrderType,
				       int _minDelay = -1);
  
  // constructor for explicit value dependence (may be true/anti/output)
  /*ctor*/		SchedGraphEdge(SchedGraphNodeCommon* _src,
				       SchedGraphNodeCommon* _sink,
				       const Value*    _val,
				       unsigned int     _depOrderType,
				       int _minDelay = -1);
  
  // constructor for machine register dependence
  /*ctor*/		SchedGraphEdge(SchedGraphNodeCommon* _src,
				       SchedGraphNodeCommon* _sink,
				       unsigned int    _regNum,
				       unsigned int     _depOrderType,
				       int _minDelay = -1);
  
  // constructor for any other machine resource dependences.
  // DataDepOrderType is always NonDataDep.  It it not an argument to
  // avoid overloading ambiguity with previous constructor.
  /*ctor*/		SchedGraphEdge(SchedGraphNodeCommon* _src,
				       SchedGraphNodeCommon* _sink,
				       ResourceId      _resourceId,
				       int _minDelay = -1);
  
  /*dtor*/		~SchedGraphEdge();
  
  SchedGraphNodeCommon*	getSrc		() const { return src; }
  SchedGraphNodeCommon*	getSink		() const { return sink; }
  int			getMinDelay	() const { return minDelay; }
  SchedGraphEdgeDepType getDepType	() const { return depType; }
  
  const Value*		getValue	() const {
    assert(depType == ValueDep); return val;
  }
  int			getMachineReg	() const {
    assert(depType == MachineRegister); return machineRegNum;
  }
  int			getResourceId	() const {
    assert(depType == MachineResource); return resourceId;
  }
  void                  setIteDiff      (int _iteDiff) {
    iteDiff = _iteDiff;
  }
  int                   getIteDiff      (){
    return iteDiff;
  }
  
public:
  // 
  // Debugging support
  // 
  friend std::ostream& operator<<(std::ostream& os, const SchedGraphEdge& edge);
  
  void		dump	(int indent=0) const;
    
private:
  // disable default ctor
  /*ctor*/		SchedGraphEdge();	// DO NOT IMPLEMENT
};


class SchedGraphCommon {
  
protected:
  SchedGraphNodeCommon* graphRoot;		// the root and leaf are not inserted
  SchedGraphNodeCommon* graphLeaf;		//  in the hash_map (see getNumNodes())

public:
  //
  // Accessor methods
  //
  SchedGraphNodeCommon*		   getRoot()	    const { return graphRoot; }
  SchedGraphNodeCommon*		   getLeaf()	    const { return graphLeaf; } 
 
  //
  // Delete nodes or edges from the graph.
  // 
  void		eraseNode		(SchedGraphNodeCommon* node);
  
  void		eraseIncomingEdges	(SchedGraphNodeCommon* node,
					 bool addDummyEdges = true);
  
  void		eraseOutgoingEdges	(SchedGraphNodeCommon* node,
					 bool addDummyEdges = true);
  
  void		eraseIncidentEdges	(SchedGraphNodeCommon* node,
					 bool addDummyEdges = true);
  
  /*ctor*/	SchedGraphCommon		();
  /*dtor*/	~SchedGraphCommon		();

};



#endif
