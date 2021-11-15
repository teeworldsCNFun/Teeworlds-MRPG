#ifndef SERVER_STDAFX_H
#define SERVER_STDAFX_H

//TODO: add precompiled headers

// core
#include <atomic>
#include <vector>
#include <list>
#include <mutex>
#include <thread>
#include <map>
#include <unordered_map>
#include <string>

// custom
#include <base/vmath.h>
#include <base/threadpool.h>
#include <generated/protocol.h>
#include <game/game_context.h>
#include <teeother/tl/nlohmann_json.h>

#endif //SERVER_STDAFX_H