#include "__dep__.h"
#include "frame.h"
#include "config.h"
#include "rococo/rcc_row.h"
#include "snow/ro6_row.h"
#include "marshal-value.h"
#include "coordinator.h"
#include "dtxn.h"
#include "rcc_service.h"
#include "scheduler.h"
#include "none/coordinator.h"
#include "none/scheduler.h"
#include "rococo/coord.h"
#include "snow/ro6_coord.h"
#include "deptran/2pl/coordinator.h"
#include "occ/tx.h"
#include "occ/coordinator.h"

#include "bench/tpcc_real_dist/sharding.h"
#include "bench/tpcc/workload.h"


// for tpca benchmark
#include "bench/tpca/workload.h"
#include "bench/tpca/payment.h"
#include "bench/tpca/sharding.h"
#include "bench/tpca/workload.h"

// tpcc benchmark
#include "bench/tpcc/workload.h"
#include "bench/tpcc/procedure.h"
#include "bench/tpcc/sharding.h"

// tpcc dist partition benchmark
#include "bench/tpcc_dist/procedure.h"

// tpcc real dist partition benchmark
#include "bench/tpcc_real_dist/workload.h"
#include "bench/tpcc_real_dist/procedure.h"

// rw benchmark
#include "bench/rw/workload.h"
#include "bench/rw/procedure.h"
#include "bench/rw/sharding.h"

// micro bench
#include "bench/micro/workload.h"
#include "bench/micro/procedure.h"

#include "deptran/2pl/scheduler.h"
#include "occ/scheduler.h"

#include "extern_c/frame.h"


namespace janus {

Frame* Frame::RegFrame(int mode,
                       function<Frame*()> frame_init) {
  auto& mode_to_frame = Frame::ModeToFrame();
  auto it = mode_to_frame.find(mode);
  verify(it == mode_to_frame.end());
  mode_to_frame[mode] = frame_init;
  return frame_init();
}

Frame* Frame::GetFrame(int mode) {
  Frame *frame = nullptr;
  // some built-in mode
  switch (mode) {
    case MODE_NONE:
    case MODE_MDCC:
    case MODE_2PL:
    case MODE_OCC:
      frame = new Frame(mode);
      break;
    case MODE_EXTERNC:
      frame = new ExternCFrame();
      break;
    default:
      auto& mode_to_frame = Frame::ModeToFrame();
      auto it = mode_to_frame.find(mode);
      verify(it != mode_to_frame.end());
      frame = it->second();
  }

  return frame;
}

int Frame::Name2Mode(string name) {
  auto &m = Frame::FrameNameToMode();
  auto it = m.find(name);
  verify(it != m.end());
  return it->second;
}

Frame* Frame::GetFrame(string name) {
  return GetFrame(Name2Mode(name));
}

Frame* Frame::RegFrame(int mode,
                       vector<string> names,
                       function<Frame*()> frame) {
  for (auto name: names) {
    //verify(frame_name_mode_s.find(name) == frame_name_mode_s.end());
    auto &m = Frame::FrameNameToMode();
    m[name] = mode;
  }
  return RegFrame(mode, frame);
}

Sharding* Frame::CreateSharding() {
  Sharding* ret;
  auto bench = Config::config_s->benchmark_;
  switch (bench) {
    case TPCC_REAL_DIST_PART:
      ret = new TpccdSharding();
      break;
    case TPCC:
      ret = new TpccSharding();
      break;
    case RW_BENCHMARK:
      ret = new RWBenchmarkSharding();
      break;
    case TPCA:
      ret = new TpcaSharding();
      break;
    default:
      verify(0);
  }
  return ret;
}

Sharding* Frame::CreateSharding(Sharding *sd) {
  verify(sd != nullptr);
  Sharding* ret = CreateSharding();
  *ret = *sd;
  ret->frame_ = this;
  return ret;
}

mdb::Row* Frame::CreateRow(const mdb::Schema *schema,
                           vector<Value> &row_data) {
//  auto mode = Config::GetConfig()->cc_mode_;
  auto mode = mode_;
  mdb::Row* r = nullptr;
  switch (mode) {
    case MODE_2PL:
      r = mdb::FineLockedRow::create(schema, row_data);
      break;
    case MODE_RO6:
      r = RO6Row::create(schema, row_data);
      break;
    case MODE_NONE: // FIXME
    case MODE_MDCC:
    case MODE_OCC:
    default:
      r = mdb::VersionedRow::create(schema, row_data);
      break;
  }
  return r;
}

Coordinator* Frame::CreateCoord(cooid_t coo_id,
                                Config* config,
                                int benchmark,
                                ClientControlServiceImpl *ccsi,
                                uint32_t id,
                                TxnRegistry* txn_reg) {
  // TODO: clean this up; make Coordinator subclasses assign txn_reg_
  Coordinator *coo;
  auto attr = this;
//  auto mode = Config::GetConfig()->cc_mode_;
  auto mode = mode_;
  switch (mode) {
    case MODE_2PL:
      coo = new Coordinator2pl(coo_id,
                         benchmark,
                         ccsi,
                         id);
      ((Coordinator*)coo)->txn_reg_ = txn_reg;
      break;
    case MODE_OCC:
    case MODE_RPC_NULL:
      coo = new CoordinatorOcc(coo_id,
                         benchmark,
                         ccsi,
                         id);
      ((Coordinator*)coo)->txn_reg_ = txn_reg;
      break;
    case MODE_RCC:
      coo = new RccCoord(coo_id,
                         benchmark,
                         ccsi,
                         id);
      ((Coordinator*)coo)->txn_reg_ = txn_reg;
      break;
    case MODE_RO6:
      coo = new RO6Coord(coo_id,
                         benchmark,
                         ccsi,
                         id);
      ((Coordinator*)coo)->txn_reg_ = txn_reg;
      break;
    case MODE_MDCC:
//      coo = (Coordinator*)new mdcc::MdccCoordinator(coo_id, id, config, ccsi);
      break;
    case MODE_NONE:
    default:
      coo = new CoordinatorNone(coo_id,
                          benchmark,
                          ccsi,
                          id);
      ((Coordinator*)coo)->txn_reg_ = txn_reg;
      break;
  }
  coo->frame_ = this;
  return coo;
}

void Frame::GetTxnTypes(std::map<int32_t, std::string> &txn_types) {
  auto benchmark_ = Config::config_s->benchmark_;
  switch (benchmark_) {
    case TPCA:
      txn_types[TPCA_PAYMENT] = std::string(TPCA_PAYMENT_NAME);
      break;
    case TPCC:
    case TPCC_DIST_PART:
    case TPCC_REAL_DIST_PART:
      txn_types[TPCC_NEW_ORDER] = std::string(TPCC_NEW_ORDER_NAME);
      txn_types[TPCC_PAYMENT] = std::string(TPCC_PAYMENT_NAME);
      txn_types[TPCC_STOCK_LEVEL] = std::string(TPCC_STOCK_LEVEL_NAME);
      txn_types[TPCC_DELIVERY] = std::string(TPCC_DELIVERY_NAME);
      txn_types[TPCC_ORDER_STATUS] = std::string(TPCC_ORDER_STATUS_NAME);
      break;
    case RW_BENCHMARK:
      txn_types[RW_BENCHMARK_W_TXN] = std::string(RW_BENCHMARK_W_TXN_NAME);
      txn_types[RW_BENCHMARK_R_TXN] = std::string(RW_BENCHMARK_R_TXN_NAME);
      break;
    case MICRO_BENCH:
      txn_types[MICRO_BENCH_R] = std::string(MICRO_BENCH_R_NAME);
      txn_types[MICRO_BENCH_W] = std::string(MICRO_BENCH_W_NAME);
      break;
    default:
      Log_fatal("benchmark not implemented");
      verify(0);
  }
}

Procedure* Frame::CreateTxnCommand(TxnRequest& req, TxnRegistry* reg) {
  auto benchmark = Config::config_s->benchmark_;
  Procedure *cmd = NULL;
  switch (benchmark) {
    case TPCA:
      verify(req.txn_type_ == TPCA_PAYMENT);
      cmd = new TpcaPaymentChopper();
      break;
    case TPCC:
      cmd = new TpccProcedure();
      break;
    case TPCC_DIST_PART:
      cmd = new TpccDistChopper();
      break;
    case TPCC_REAL_DIST_PART:
      cmd = new TpccRdProcedure();
      break;
    case RW_BENCHMARK:
      cmd = new RWChopper();
      break;
    case MICRO_BENCH:
      cmd = new MicroProcedure();
      break;
    default:
      verify(0);
  }
  verify(cmd != NULL);
  cmd->txn_reg_ = reg;
  cmd->sss_ = Config::GetConfig()->sharding_;
  cmd->Init(req);
  verify(cmd->n_pieces_dispatchable_ > 0);
  return cmd;
}

Procedure * Frame::CreateChopper(TxnRequest &req, TxnRegistry* reg) {
  return CreateTxnCommand(req, reg);
}

Communicator* Frame::CreateCommo(PollMgr* pollmgr) {
  commo_ = new RococoCommunicator;
  return commo_;
}

shared_ptr<Tx> Frame::CreateTx(epoch_t epoch, txnid_t tid,
                    bool ro, Scheduler *mgr) {
  shared_ptr<Tx> sp_tx;

  switch (mode_) {
    case MODE_2PL:
      sp_tx.reset(new Tx2pl(epoch, tid, mgr));
      break;
    case MODE_OCC:
      sp_tx.reset(new TxOcc(epoch, tid, mgr));
      break;
    case MODE_RCC:
      sp_tx.reset(new RccDTxn(epoch, tid, mgr, ro));
      break;
    case MODE_RO6:
      sp_tx.reset(new RO6DTxn(tid, mgr, ro));
      break;
    case MODE_MULTI_PAXOS:
      break;
    case MODE_NONE:
    default:
      sp_tx.reset(new Tx2pl(epoch, tid, mgr));
      break;
  }
  return sp_tx;
}

Executor* Frame::CreateExecutor(cmdid_t cmd_id, Scheduler* sched) {
  Executor* exec = nullptr;
//  auto mode = Config::GetConfig()->cc_mode_;
//  switch (mode) {
//    case MODE_NONE:
//      verify(0);
//    case MODE_2PL:
//      exec = new TplExecutor(cmd_id, sched);
//      break;
//    case MODE_OCC:
//      exec = new OCCExecutor(cmd_id, sched);
//      break;
//    default:
//      verify(0);
//  }
  return exec;
}

Scheduler* Frame::CreateScheduler() {
  auto mode = Config::GetConfig()->cc_mode_;
  Scheduler *sch = nullptr;
  switch(mode) {
    case MODE_2PL:
      sch = new Scheduler2pl();
      break;
    case MODE_OCC:
      sch = new SchedulerOcc();
      break;
    case MODE_MDCC:
//      sch = new mdcc::MdccScheduler();
      break;
    case MODE_NONE:
      sch = new SchedulerNone();
      break;
    case MODE_RPC_NULL:
    case MODE_RCC:
    case MODE_RO6:
      verify(0);
      break;
    default:
      verify(0);
//      sch = new CustomSched();
  }
  verify(sch);
  sch->frame_ = this;
  return sch;
}

Workload * Frame::CreateTxnGenerator() {
  auto benchmark = Config::config_s->benchmark_;
  Workload * gen = nullptr;
  switch (benchmark) {
    case TPCC:
      gen = new TpccWorkload(Config::GetConfig());
      break;
    case TPCC_DIST_PART:
    case TPCC_REAL_DIST_PART:
      gen = new TpccRdWorkload(Config::GetConfig());
      break;
    case TPCA:
      gen = new TpcaWorkload(Config::GetConfig());
      break;
    case RW_BENCHMARK:
      gen = new RwWorkload(Config::GetConfig());
      break;
    case MICRO_BENCH:
    default:
      verify(0);
  }
  return gen;
}

vector<rrr::Service *> Frame::CreateRpcServices(uint32_t site_id,
                                                Scheduler *dtxn_sched,
                                                rrr::PollMgr *poll_mgr,
                                                ServerControlServiceImpl *scsi) {
  auto config = Config::GetConfig();
  auto result = std::vector<Service *>();
  switch(mode_) {
    case MODE_MDCC:
//      result.push_back(new mdcc::MdccClientServiceImpl(config,
//                                                       site_id,
//                                                       dtxn_sched));
//      result.push_back(new mdcc::MdccLeaderServiceImpl(config,
//                                                       site_id,
//                                                       dtxn_sched));
//      result.push_back(new mdcc::MdccAcceptorServiceImpl(config,
//                                                         site_id,
//                                                         dtxn_sched));
//      result.push_back(new mdcc::MdccLearnerServiceImpl(config,
//                                                        site_id,
//                                                        dtxn_sched));
      break;
    case MODE_2PL:
    case MODE_OCC:
    case MODE_NONE:
    case MODE_TAPIR:
    case MODE_JANUS:
    case MODE_RCC:
    default:
      result.push_back(new ClassicServiceImpl(dtxn_sched, poll_mgr, scsi));
      break;
  }
  return result;
}
map<string, int> &Frame::FrameNameToMode() {
  static map<string, int> frame_name_mode_s = {
      {"none",          MODE_NONE},
      {"2pl",           MODE_2PL},
      {"occ",           MODE_OCC},
      {"snow",           MODE_RO6},
      {"rpc_null",      MODE_RPC_NULL},
      {"deptran",       MODE_DEPTRAN},
      {"deptran_er",    MODE_DEPTRAN},
      {"2pl_w",         MODE_2PL},
      {"2pl_wait_die",  MODE_2PL},
      {"2pl_wd",        MODE_2PL},
      {"2pl_ww",        MODE_2PL},
      {"2pl_wound_die", MODE_2PL},
      {"externc",       MODE_EXTERNC},
      {"extern_c",      MODE_EXTERNC},
      {"mdcc",          MODE_MDCC},
      {"multi_paxos",   MODE_MULTI_PAXOS},
      {"epaxos",        MODE_NOT_READY},
      {"rep_commit",    MODE_NOT_READY}
  };
  return frame_name_mode_s;
}

map<int, function<Frame*()>> &Frame::ModeToFrame() {
  static map<int, function<Frame*()>> frame_s_ = {};
  return frame_s_;
}
} // namespace rococo;
