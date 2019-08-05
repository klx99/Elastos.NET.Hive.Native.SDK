
#include <iostream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <random>

#include <hive/cluster.h>
#include <hive/node.h>
#include <hive/test/utils.h>
#include "hive/message.h"

bool DStore::select_node() {
  std::random_device rd;
  std::mt19937 g(rd());
  // Please replace this line
  std::string uid = "uid-2041b18e-ca86-4962-9a21-d477f7f627ce";

  std::shuffle(candidate_nodes.begin(), candidate_nodes.end(), g);

  for (auto &nd : candidate_nodes) {
    if (!nd.ipv4.empty()) {
      node.set_addr(nd.ipv4, nd.port);
      try {
        ipfs::Json json;
        node.Version(&json);
        return set_sender_UID(uid);
      } catch (...) {}
    }

    if (!nd.ipv6.empty()) {
      node.set_addr(nd.ipv6, nd.port);
      try {
        ipfs::Json json;
        node.Version(&json);
        return set_sender_UID(uid);
      } catch (...) {}
    }
  }

  return false;
}

DStore::DStore(std::vector<dstore_node> &&nodes, bool log): candidate_nodes {nodes} {
  debugLog = log;
  needPublish = true;
  needResolve = true;

  if (!select_node())
    throw std::runtime_error("no IPFS node is reachable");
}

std::string DStore::get_peerId() { return peerId; }

std::shared_ptr<std::vector<std::string>> DStore::get_keys() {
  // ipfs name publish --key=mykey
  //  ipfs name resolve QmRMgVUVsVcBCo8XMnsfMtLMaQ1myGWnhv8xy6iCTBccyZ
  // /ipfs/QmWMg1Wm6pyjU6VWrBU3uvY9kfm17X3UbHz91qm2wY559j

  //   YIMAC:cpp-test yiwang$ ipfs ls
  //   /ipns/QmYBvxNNXfjafsV7s88fekuRfkqNNRmB4stur88KsaSsaA
  // QmdFASUwf5ab5pY5ucpamGPAe2uLzLYtksR284uTtYyWne 123 001-001.txt
  // QmcFUHo1DBpydnTSeuXszKgQqnXQCJq6Wu1BUdzEfuRahM 64  001.txt

  // {"Arguments":{"/ipns/QmYBvxNNXfjafsV7s88fekuRfkqNNRmB4stur88KsaSsaA":"Qmf7VYPRhr5v17No8Ppsgo6ShZFHe7J7ukH1QRmqBxA1wZ"},"Objects":{"Qmf7VYPRhr5v17No8Ppsgo6ShZFHe7J7ukH1QRmqBxA1wZ":{"Hash":"Qmf7VYPRhr5v17No8Ppsgo6ShZFHe7J7ukH1QRmqBxA1wZ","Size":0,"Type":"Directory","Links":[{"Name":"001-001.txt","Hash":"QmdFASUwf5ab5pY5ucpamGPAe2uLzLYtksR284uTtYyWne","Size":13,"Type":"File"},{"Name":"001.txt","Hash":"QmcFUHo1DBpydnTSeuXszKgQqnXQCJq6Wu1BUdzEfuRahM","Size":6,"Type":"File"}]}}}

  // ipfs files ls /nodes/peerId/
  // ipfs files read /nodes/peerId/messages/key/001
  // ipfs files read /nodes/peerId/messages/key/002
  // ..

  std::shared_ptr<std::vector<std::string>> vm(new std::vector<std::string>);
  try {
    ipfs::Json ls_result;
    std::string path = workingPath + "/messages/";

    node.FileLs(path, &ls_result);
    std::string objectsId = ls_result["Arguments"][path];
    ipfs::Json& messages = ls_result["Objects"][objectsId]["Links"];
    for (ipfs::Json::iterator it = messages.begin(); it != messages.end();
         ++it) {
      const ipfs::Json& message = it.value();

      std::string key = message["Name"].get<std::string>();
      vm->push_back(key);
    }
  } catch (const std::exception& e) {
    if (debugLog) {
      std::cerr << e.what() << std::endl;
    }
  }

  return vm;
}

std::shared_ptr<std::vector<std::string>> DStore::get_remote_keys(
    const std::string& peerId) {
  std::shared_ptr<std::vector<std::string>> vm(new std::vector<std::string>);

  try {
    ipfs::Json ls_result;
    std::string path = "/ipns/" + peerId + "/messages/";

    node.FileLs(path, &ls_result);
    std::string objectsId = ls_result["Arguments"][path];
    ipfs::Json& messages = ls_result["Objects"][objectsId]["Links"];
    if (debugLog) {
      // std::cerr << "result json: " << std::endl << ls_result.dump(4) <<
      // std::endl;
    }
    for (ipfs::Json::iterator it = messages.begin(); it != messages.end();
         ++it) {
      const ipfs::Json& message = it.value();

      std::string key = message["Name"].get<std::string>();
      vm->push_back(key);
    }
  } catch (const std::exception& e) {
    if (debugLog) {
      std::cerr << e.what() << std::endl;
    }
  }

  return vm;
}

std::shared_ptr<std::vector<std::shared_ptr<DMessage>>> DStore::get_values(
    const std::string& key) {
  auto dmsg = get_values_internal(key);

  if (dmsg)
    return dmsg;

  if (!select_node())
    return nullptr;

  return get_values_internal(key);
}

std::shared_ptr<std::vector<std::shared_ptr<DMessage>>> DStore::get_values_internal(
    const std::string &key) {
  // ipfs files ls /nodes/peerId/
  // ipfs files read /nodes/peerId/messages/key/001
  // ipfs files read /nodes/peerId/messages/key/002
  // ..

  std::shared_ptr<std::vector<std::shared_ptr<DMessage>>> vm(
      new std::vector<std::shared_ptr<DMessage>>);
  ipfs::Json ls_result;
  std::string path = workingPath + "/messages/" + key;
  ipfs::Json messages;

  try {
    node.FilesLs(userId, path, &ls_result);
    messages = ls_result["Entries"];
  } catch (const std::exception& e) {
    if (debugLog) {
      std::cerr << e.what() << std::endl;
      std::cerr << "DEBUG: List key path " << path << " for key " << key
                << " failed" << std::endl;
    }
    return vm;
  }

  if (debugLog) {
    std::cerr << "DEBUG: List key path " << path << " for key " << key
              << " successfully" << std::endl;
  }

  try {
    for (ipfs::Json::iterator it = messages.begin(); it != messages.end();
         ++it) {
      const ipfs::Json& message = it.value();
      const std::string name = message["Name"].get<std::string>();

      std::stringstream content;
      node.FilesRead(userId, path + "/" + name, 0, 1000, &content);

      std::shared_ptr<DMessage> dmsg(new DMessage(name, content.str()));
      vm->push_back(dmsg);
    }
  } catch (const std::exception& e) {
    if (debugLog) {
      std::cerr << "DEBUG: Read key path " << path << " for key " << key
                << " failed" << std::endl;
      std::cerr << e.what() << std::endl;
    }
    return nullptr;
  }

  if (debugLog) {
    std::cerr << "DEBUG: Read key path " << path << " for key " << key
              << " successfully" << std::endl;
  }

  return vm;
}

std::shared_ptr<std::vector<std::shared_ptr<DMessage>>>
DStore::get_remote_values(const std::string& peerId, const std::string& key) {
  std::shared_ptr<std::vector<std::shared_ptr<DMessage>>> vm(
      new std::vector<std::shared_ptr<DMessage>>);
  ipfs::Json ls_result;
  std::string path = "/ipns/" + peerId + "/messages/" + key;
  ipfs::Json messages;

  try {
    node.FileLs(path, &ls_result);
    std::string objectsId = ls_result["Arguments"][path];
    messages = ls_result["Objects"][objectsId]["Links"];
  } catch (const std::exception& e) {
    if (debugLog) {
      std::cerr << e.what() << std::endl;
      std::cerr << "DEBUG: List key path " << path << " for key " << key
                << " failed" << std::endl;
    }
    return vm;
  }

  if (debugLog) {
    std::cerr << "DEBUG: List key path " << path << " for key " << key
              << " successfully" << std::endl;
  }

  try {
    for (ipfs::Json::iterator it = messages.begin(); it != messages.end();
         ++it) {
      const ipfs::Json& message = it.value();
      const std::string name = message["Name"].get<std::string>();

      std::stringstream content;
      node.FileGet(path + "/" + name, &content);

      std::shared_ptr<DMessage> dmsg(new DMessage(name, content.str()));
      vm->push_back(dmsg);
    }
  } catch (const std::exception& e) {
    if (debugLog) {
      std::cerr << "DEBUG: Read key path " << path << " for key " << key
                << " failed" << std::endl;
      std::cerr << e.what() << std::endl;
    }
    return vm;
  }

  if (debugLog) {
    std::cerr << "DEBUG: Read key path " << path << " for key " << key
              << " successfully" << std::endl;
  }

  return vm;
}

bool DStore::add_value(const std::string& key,
                       std::shared_ptr<DMessage> message) {
  if (add_value_internal(key, message))
    return true;

  if (!select_node())
    return false;

  return add_value_internal(key, message);
}

bool DStore::add_value_internal(const std::string &key,
                                std::shared_ptr<DMessage> message) {
  // > ipfs files write /nodes/peerId/messages/key/timestamp-xxxx

  // > ipfs files stat  /nodes/peerId
  // xxxxxxxxx
  // > ipfs name  publish --key=uid-132fdc89-4e85-47f5-b942-fd7ba515a6f3
  // /ipfs/xxxxxxxxx /ipfs/Qmf7VYPRhr5v17No8Ppsgo6ShZFHe7J7ukH1QRmqBxA1wZ
  // /ipfs/QmatmE9msSfkKxoffpHwNLNKgwZG8eT9Bud6YoPab52vpy

  ipfs::Json result;
  std::string path = workingPath + "/messages/" + key;
  bool keyPath = true;

  try {
    node.FilesLs(userId, path, &result);
  } catch (const std::exception& e) {
    if (debugLog) {
      std::cerr << "Not exist path: " << path << ". Creating it ... "
                << e.what() << std::endl;
    }
    keyPath = false;
  }

  // std::cout << "DEBUG: " << result.dump() << std::endl;

  if (!keyPath) {
    try {
      node.FilesMkdir(userId, path, &result);
    } catch (const std::exception& e) {
      if (debugLog) {
        std::cerr << "Mkdir: " << path << "; " << e.what() << std::endl;
      }
      return false;
    }
  }
  if (debugLog) {
    std::cerr << "DEBUG: Create key path " << path << " for key " << key
              << " successfully" << std::endl;
  }

  std::string messagePath = path + "/" + message->timestamp();
  try {
    ipfs::Json result0;
    node.FilesWrite(userId, messagePath, message->value(), 0, true, false,
                    &result0);
  } catch (const std::exception& e) {
    if (debugLog) {
      std::cerr << __LINE__ << " FilesWrite: " << e.what() << std::endl;
    }
    return false;
  }

  if (debugLog) {
    std::cerr << "DEBUG: Write message \"" << message->value() << "\" for key "
              << key << " is success, publish now... " << std::endl;
  }

  needPublish = true;
  return publish();
}

bool DStore::remove_values(const std::string& key) {
  if (remove_values_internal(key))
    return true;

  if (!select_node())
    return false;

  return remove_values_internal(key);
}

bool DStore::remove_values_internal(const std::string& key) {
  // > ipfs files rm /nodes/peerId/messages/key/xxxxxxxx

  // > ipfs files stat /nodes/peerId
  // xxxxxxxx
  // > ipfs name  publish --key=uid-132fdc89-4e85-47f5-b942-fd7ba515a6f3
  // /ipfs/xxxxxxxxx /ipfs/QmatmE9msSfkKxoffpHwNLNKgwZG8eT9Bud6YoPab52vpy

  try {
    ipfs::Json result;
    std::string path = workingPath + "/messages/" + key;

    node.FilesRm(userId, path, true, &result);
  } catch (const std::exception& e) {
    if (debugLog) {
      std::cerr << e.what() << std::endl;
    }
    return false;
  }

  if (debugLog) {
    std::cerr << "DEBUG: Remove messages for key " << key
              << " is success, publish now... " << std::endl;
  }

  needPublish = true;
  return publish();
}

bool DStore::set_sender_UID(std::string &uid){
  ipfs::Json uidInfo;

  node.UidInfo(uid, &uidInfo);
  userId = uidInfo["UID"].get<std::string>();
  peerId = uidInfo["PeerID"].get<std::string>();

  if (debugLog) {
    std::cerr << "My PeerID is " << peerId << std::endl;
  }

  // > ipfs files mkdir /nodes/peerId/
  // > ipfs name resolve /ipns/peerId
  // /ipfs/xxxxxxx
  // > ipfs files mv /ipfs/xxxxxxx/messages /nodes/peerId/

  workingPath = "/";
  ipfs::Json result;

  try {
    node.FilesRm(userId, workingPath + "messages", true, &result);
  } catch (const std::exception& e) {
    if (debugLog) {
      std::cerr << e.what() << std::endl;
    }
  }

  if (debugLog) {
    std::cerr << "Cleanuped workingPath: " << (workingPath + "messages")
              << std::endl;
  }

  needResolve = true;
  resolve();

  // verify working path
  try {
    node.FilesLs(userId, workingPath, &result);
  } catch (const std::exception& e) {
    if (debugLog) {
      std::cerr << e.what() << std::endl;
      std::cerr << "Please call UIDNew()  " << std::endl;
    }
    return false;
  }

  bool messagesPath = false;
  for (ipfs::Json::iterator it = result["Entries"].begin();
       it != result["Entries"].end(); ++it) {
    const ipfs::Json& key = it.value();

    if (key["Name"] == "messages") {
      messagesPath = true;
      break;
    }
  }

  if (!messagesPath) {
    try {
      node.FilesMkdir(userId, workingPath + "messages", &result);
    } catch (const std::exception& e) {
      if (debugLog) {
        std::cerr << e.what() << std::endl;
      }
      return false;
    }
  }

  return true;
}

void DStore::enable_log(bool enable) { debugLog = enable; }

bool DStore::publish() {
  // > ipfs files stat /nodes/peerId
  // xxxxxxxx
  // > ipfs name  publish --key=uid-132fdc89-4e85-47f5-b942-fd7ba515a6f3
  // /ipfs/xxxxxxxxx /ipfs/QmatmE9msSfkKxoffpHwNLNKgwZG8eT9Bud6YoPab52vpy

  ipfs::Json result;

  if (needPublish) {
    try {
      node.FilesStat(userId, workingPath, &result);
    } catch (const std::exception& e) {
      if (debugLog) {
        std::cerr << "publish at FilesStat: " << e.what() << std::endl;
      }
      return false;
    }

    std::string hash = result["Hash"].get<std::string>();
    try {
      node.NamePublish(userId, "/ipfs/" + hash, &result);

      if (debugLog) {
        std::cerr << "published with userId:" << userId
                  << " path = " << workingPath << " and hash = " << hash
                  << std::endl;
      }

      needPublish = false;
    } catch (const std::exception& e) {
      if (debugLog) {
        std::cerr << "publish failed, userId:" << userId
                  << " path = " << workingPath << " and hash = " << hash << " "
                  << e.what() << std::endl;
      }
      return false;
    }
  }

  return true;
}

bool DStore::resolve() {
  // > ipfs name resolve /ipns/peerId
  // /ipfs/xxxxxxx
  // > ipfs files mv /ipfs/xxxxxxx/messages /nodes/peerId/

  ipfs::Json result;

  if (needResolve) {
    try {
      node.NameResolve("/ipns/" + peerId, &result);
      rootPath = result["Path"].get<std::string>();
    } catch (const std::exception& e) {
      if (debugLog) {
        std::cerr << "NameResolve: " << e.what() << std::endl;
      }
      return false;
    }

    if (debugLog) {
      std::cerr << "DEBUG: Fetched rootPath " << rootPath << " for peerId "
                << peerId << " successfully" << std::endl;
    }

    try {
      node.FilesCp(userId, rootPath + "/messages", workingPath + "/messages", &result);
    } catch (const std::exception& e) {
      if (debugLog) {
        std::cerr << "DEBUG: recover workingPath " << workingPath << " from "
                  << (rootPath + "/messages") << " for peerId " << peerId
                  << " failed. Continue ..." << std::endl;
        std::cerr << "FilesCp: " << e.what() << std::endl;
      }
      return false;
    }

    if (debugLog) {
      std::cerr << "DEBUG: recover workingPath " << workingPath
                << " for peerId " << peerId << " successfully" << std::endl;
    }

    needResolve = false;
  }

  return true;
}

std::string get_timestamp() {
  std::time_t result = std::time(nullptr);
  return std::to_string(result);
}
