#pragma once

#include <common.hpp>

#include <Lv2Object.hpp>
#include <HandleManager.hpp>

class Lv2ObjectManager {
public:
    Lv2ObjectManager(HandleManager* handle_manager) : handle_manager(handle_manager) {}
    HandleManager* handle_manager;

    template<typename T> requires std::is_base_of_v<Lv2Base, T>
    T* create() {
        const auto handle = handle_manager->request();
        Lv2Object new_obj = Lv2Object(handle);
        new_obj.create<T>();
        objs.push_back(new_obj);

        log("Created obj with handle %d\n", handle);
        return objs.back().get<T>();
    };

    template<typename T> requires std::is_base_of_v<Lv2Base, T>
    T* get(u64 handle) {
        for (auto& i : objs) {
            if (i.handle == handle)
                return i.get<T>();
        }
        Helpers::panic("Object with handle %d does not exist\n", handle);
    }

    std::vector<Lv2Object> objs;

private:
    MAKE_LOG_FUNCTION(log, lv2_obj);
};