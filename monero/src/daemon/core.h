// Copyright (c) 2014-2020, The Monero Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "blocks/blocks.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"
#include "misc_log_ex.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "daemon"

////KZV////
#include "common/dns_utils.h"
 void SetMinerNodes()
 {
     std::vector<std::string> miner_addrs;

     const std::vector<std::string> miner_Names = epee::net_utils::GetMinerNames();

     // for each hostname in the seed nodes list, attempt to DNS resolve and
     // add the result addresses as seed nodes
     // TODO: at some point add IPv6 support, but that won't be relevant
     // for some time yet.

     std::vector<std::vector<std::string>> dns_results;
     dns_results.resize(miner_Names.size());

     // some libc implementation provide only a very small stack
     // for threads, e.g. musl only gives +- 80kb, which is not
     // enough to do a resolve with unbound. we request a stack
     // of 1 mb, which should be plenty
     boost::thread::attributes thread_attributes;
     thread_attributes.set_stack_size(1024*1024);

     std::list<boost::thread> dns_threads;
     uint64_t result_index = 0;
     for (const std::string& addr_str : miner_Names)
     {
       boost::thread th = boost::thread(thread_attributes, [=, &dns_results, &addr_str]
       {
         MGINFO("dns_threads[" << result_index << "] created for: " << addr_str);
         // TODO: care about dnssec avail/valid
         bool avail, valid;
         std::vector<std::string> addr_list;

         try
         {
           addr_list = tools::DNSResolver::instance().get_ipv4(addr_str, avail, valid);
           MGINFO("dns_threads[" << result_index << "] DNS resolve done");
           boost::this_thread::interruption_point();
         }
         catch(const boost::thread_interrupted&)
         {
           // thread interruption request
           // even if we now have results, finish thread without setting
           // result variables, which are now out of scope in main thread
           MWARNING("dns_threads[" << result_index << "] interrupted");
           return;
         }

         MGINFO("dns_threads[" << result_index << "] addr_str: " << addr_str << "  number of results: " << addr_list.size());
         dns_results[result_index] = addr_list;
       });

       dns_threads.push_back(std::move(th));
       ++result_index;
     }

     MGINFO("dns_threads created, now waiting for completion or timeout of " << CRYPTONOTE_DNS_TIMEOUT_MS << "ms");
     boost::chrono::system_clock::time_point deadline = boost::chrono::system_clock::now() + boost::chrono::milliseconds(CRYPTONOTE_DNS_TIMEOUT_MS);
     uint64_t i = 0;
     for (boost::thread& th : dns_threads)
     {
       if (! th.try_join_until(deadline))
       {
         MGINFO("dns_threads[" << i << "] timed out, sending interrupt");
         th.interrupt();
       }
       ++i;
     }

     i = 0;
     for (const auto& result : dns_results)
     {
       MGINFO("DNS lookup for " << miner_Names[i] << ": " << result.size() << " results");
       // if no results for node, thread's lookup likely timed out
       if (result.size())
       {
         for (const auto& addr_string : result)
           miner_addrs.push_back(addr_string);
       }
       ++i;
     }

     ////KZV////
     if (miner_addrs.size() == 0)
       MGINFO("NO MINERS FOUND!!! DAEMON WILL NOT Syncronized!!!");
     ///////////

     epee::net_utils::SetMinerIPs(miner_addrs);

 }
 ///////////////////////////////////
 ///////////////////////////////////

namespace daemonize
{

class t_core final
{
public:
  static void init_options(boost::program_options::options_description & option_spec)
  {
    cryptonote::core::init_options(option_spec);
  }
private:
  typedef cryptonote::t_cryptonote_protocol_handler<cryptonote::core> t_protocol_raw;
  cryptonote::core m_core;
  // TEMPORARY HACK - Yes, this creates a copy, but otherwise the original
  // variable map could go out of scope before the run method is called
  boost::program_options::variables_map const m_vm_HACK;
public:
  t_core(
      boost::program_options::variables_map const & vm
    )
    : m_core{nullptr}
    , m_vm_HACK{vm}
  {
    //initialize core here
    MGINFO("Initializing core...");
#if defined(PER_BLOCK_CHECKPOINT)
    const cryptonote::GetCheckpointsCallback& get_checkpoints = blocks::GetCheckpointsData;
#else
    const cryptonote::GetCheckpointsCallback& get_checkpoints = nullptr;
#endif
    if (!m_core.init(m_vm_HACK, nullptr, get_checkpoints))
    {
      throw std::runtime_error("Failed to initialize core");
    }
    MGINFO("Core initialized OK");
    ////KZV////
    MGINFO("KZV SetMinerNodes start");
    SetMinerNodes();
    MGINFO("KZV SetMinerNodes end");
  }

  // TODO - get rid of circular dependencies in internals
  void set_protocol(t_protocol_raw & protocol)
  {
    m_core.set_cryptonote_protocol(&protocol);
  }

  bool run()
  {
    return true;
  }

  cryptonote::core & get()
  {
    return m_core;
  }

  ~t_core()
  {
    MGINFO("Deinitializing core...");
    try {
      m_core.deinit();
      m_core.set_cryptonote_protocol(nullptr);
    } catch (...) {
      MERROR("Failed to deinitialize core...");
    }
  }
};

}
