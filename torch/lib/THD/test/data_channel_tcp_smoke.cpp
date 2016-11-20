/*
 * This test is prone to race conditions on acquiring socket and port for listening.
 * To avoid this problem each worker waits some predefined time to let others
 * do their initial work. It is very unlikely such situation will ever occur
 * but this design does **NOT** prevent race conditions.
 *
 * Race conditions on ENV variables have been eliminated by using mutex and
 * reading all ENV variables in DataChannelTCP constructor instead of `init`
 * function where all blocking accept/connect logic is defined.
 */


#include "../base/channels/DataChannelTCP.hpp"
#include "../base/tensors/THTensor.hpp"

#include <cassert>
#include <iostream>
#include <thread>

constexpr int WORKERS_NUM = 2;
constexpr int MASTER_PORT = 45678;

std::vector<std::thread> g_all_workers;
std::mutex g_mutex;

void master()
{
  g_mutex.lock();
  setenv("WORLD_SIZE", std::to_string((WORKERS_NUM + 1)).data(), 1);
  setenv("RANK", "0", 1);
  setenv("MASTER_PORT", std::to_string(MASTER_PORT).data(), 1);
  auto masterChannel = std::make_shared<thd::DataChannelTCP>(); // reads all env variable
  g_mutex.unlock();

  assert(masterChannel->init());
  assert(masterChannel->getRank() == 0);
  assert(masterChannel->getNumProcesses() == WORKERS_NUM + 1);

  FloatTensor *float_tensor = new THTensor<float>();
  float_tensor->resize({1, 2, 3});
  float_tensor->fill(4);

  // we cannot send to ourselves
  bool thrown = false;
  try {
    masterChannel->send(*float_tensor, 0);
  } catch (const std::logic_error& e) {
    thrown = true;
  }
  assert(thrown);

  // send good tensor
  masterChannel->send(*float_tensor, 1);

  // send tensor with different sizes which does not match worker tensor sizes
  float_tensor->resize({1, 2, 3, 4});
  masterChannel->send(*float_tensor, 1);

  // broadcast int tensor
  IntTensor *int_tensor = new THTensor<int>();
  int_tensor->resize({1, 2, 3, 4, 5});
  int_tensor->fill(1000000000);

  masterChannel->broadcast(*int_tensor, 0);

  // wait for all workers to finish
  for (auto& worker : g_all_workers) {
    worker.join();
  }

  delete float_tensor;
  delete int_tensor;
}

void worker(int id)
{
  g_mutex.lock();
  setenv("RANK", std::to_string(id).data(), 1);
  setenv("MASTER_ADDR", std::string("127.0.0.1:" + std::to_string(MASTER_PORT)).data(), 1);
  auto workerChannel = std::make_shared<thd::DataChannelTCP>(); // reads all env variable
  g_mutex.unlock();

  /*
   * Wait for other processes to initialize.
   * It is to avoid race in acquiring socket and port for listening (in init function).
   */
  std::this_thread::sleep_for(std::chrono::milliseconds(100 * workerChannel->getRank()));
  assert(workerChannel->init());
  assert(workerChannel->getRank() == id);
  assert(workerChannel->getNumProcesses() == WORKERS_NUM + 1);

  FloatTensor *float_tensor = new THTensor<float>();
  float_tensor->resize({1, 2, 3});

  if (workerChannel->getRank() == 1) {
    // receive good tensor
    workerChannel->receive(*float_tensor, 0);

    for (int i = 0; i < float_tensor->numel(); i++) {
      assert(((float*)float_tensor->data())[i] == 4);
    }

    // new sizes does not match
    bool thrown = false;
    try {
      workerChannel->receive(*float_tensor, 0);
    } catch (const std::logic_error& e) {
      thrown = true;
    }
    assert(thrown);
  }

  // get broadcasted tensor
  IntTensor *int_tensor = new THTensor<int>();
  int_tensor->resize({1, 2, 3, 4, 5});

  workerChannel->broadcast(*int_tensor, 0);

  for (int i = 0; i < int_tensor->numel(); i++) {
    assert(((int*)int_tensor->data())[i] == 1000000000);
  }

  delete float_tensor;
  delete int_tensor;
}


int main() {
  // start master
  std::thread master_thread(master);

  // start worker
  for (int id = 1; id <= WORKERS_NUM; ++id) {
    g_all_workers.push_back(std::thread(worker, id));
  }

  master_thread.join();
  std::cout << "OK" << std::endl;
  return 0;
}
