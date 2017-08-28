#pragma once

#include "graph.h"
#include "dtxn.h"
#include "marshal-value.h"
#include "command.h"
#include "command_marshaler.h"
#include "../marshallable.h"

/**
 * This is NOT thread safe!!!
 */
namespace rococo {

//typedef RccDTxn RccDTxn;
typedef vector<RccDTxn*> RccScc;

class EmptyGraph : public Marshallable {
 public:
  EmptyGraph() : Marshallable(MarshallDeputy::EMPTY_GRAPH) {};
  virtual Marshal& ToMarshal(Marshal& m) const {return m;};
  virtual Marshal& FromMarshal(Marshal& m) {return m;};
};

class SchedulerRococo;
class RccGraph : public Graph<RccDTxn> {
 public:
//    Graph<PieInfo> pie_gra_;
//  Graph <TxnInfo> txn_gra_;
  SchedulerRococo* sched_{nullptr};
  bool empty_{false};
  parid_t partition_id_ = 0; // TODO
//  std::vector<rrr::Client *> rpc_clients_;
//  std::vector<RococoProxy *> rpc_proxies_;
//  std::vector<std::string> server_addrs_;

  RccGraph() : Graph<RccDTxn>() {
    kind_ = MarshallDeputy::RCC_GRAPH;
  }

  virtual ~RccGraph() {
    // XXX hopefully some memory leak here does not hurt. :(
  }

  /** on start_req */
  shared_ptr<RccDTxn> FindOrCreateRccVertex(txnid_t txn_id,
                                            SchedulerRococo* sched);
  void RemoveVertex(txnid_t txn_id);
  void RebuildEdgePointer(map<txnid_t, shared_ptr<RccDTxn>>& index);
  shared_ptr<RccDTxn> AggregateVertex(shared_ptr<RccDTxn> rhs_dtxn);
  void UpgradeStatus(RccDTxn& v, int8_t status);

  map<txnid_t, shared_ptr<RccDTxn>> Aggregate(epoch_t epoch, RccGraph& graph);
  void SelectGraphCmtUkn(RccDTxn& dtxn, RccGraph* new_graph);
  void SelectGraph(set<shared_ptr<RccDTxn>> vertexes, RccGraph* new_graph);
//  RccScc& FindSCC(RccDTxn *vertex) override;
  bool AllAncCmt(RccDTxn& vertex);

  bool operator== (RccGraph& rhs) const;

  bool operator!= (RccGraph& rhs) const {
    // TODO
    return !(*this == rhs);
  }

  uint64_t MinItfrGraph(RccDTxn& dtxn,
                        RccGraph* gra_m,
                        bool quick = false,
                        int depth = -1);

  bool HasICycle(const RccScc& scc);


//  Marshal& ToMarshal(Marshal& m) const override;
//  Marshal& FromMarshal(Marshal& m) override;

};
} // namespace rococo
