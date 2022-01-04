#pragma once
#include "common/Typedefs.h"
#include "util/Hash.h"
#include <unordered_map>
#include <stdexcept>
#include <optional>
#include <vector>
#include <variant>
#include <limits>
#include "Log.h"
#include <span>

class Property;
class Object;
class PropertyHandle;
class ObjectHandle;

//Value storage for Property
using PropertyValue = std::variant<i32, u64, u32, u16, u8, f32, bool, string, void*, std::vector<ObjectHandle>>;
static const u64 NullUID = std::numeric_limits<u64>::max();

//Global data registry. Contains objects which consist of properties.
//The object format focuses on ease of editing instead of performance. Tracks edits for undo/redo.
//Used by tools and UI so all code works with the same format instead of needing unique code per RFG format.
class Registry
{
public:
    //Get global instance of the registry. You should use this to access it. You shouldn't create your own instance.
    static Registry& Get();
    //Create registry object. Returns a handle to the object if it succeeds, or InvalidHandle if it fails.
    ObjectHandle CreateObject(std::string_view objectName = "", std::string_view typeName = "");
    bool ObjectExists(u64 uid) const;

private:
    //Registry objects mapped to their UIDs
    //Note: If the container type is changed ObjectHandle and PropertyHandle likely won't be able to hold pointers anymore.
    //      They rely on the fact that pointers to std::unordered_map values are stable.
    std::unordered_map<u64, Object> _objects = {};
    u64 _nextUID = 0;

    Object& GetObjectByUID(u64 uid);

    friend class ObjectHandle;
    friend class PropertyHandle;
};

class Property
{
public:
    string Name;
    PropertyValue Value;
};

class Object
{
public:
    string Name;
    u64 UID;
    u64 ParentUID;
    std::vector<Property> Properties;
    std::vector<u64> SubObjects;
    //Locked when editing properties. Temporary bandaid. The current design is going to be tested on the map editor first.
    //Afterwards a better solution for thread safety and any other major design flaws found will be fixed.
    std::unique_ptr<std::mutex> Mutex = std::make_unique<std::mutex>();

    Object() { }
    Object(std::string_view name, u64 uid, u64 parentUID = NullUID) : Name(name), UID(uid), ParentUID(parentUID)
    {

    }
};

//Handle to a property of a registry object
class PropertyHandle
{
public:
    PropertyHandle(Object* object, u32 index) : _object(object), _index(index) { }
    bool Valid() const { return _object && _object->UID != NullUID; }
    //Allows checking handle validity with if statements. E.g. if (handle) { /*valid*/ } else { /*invalid*/ }
    explicit operator bool() const { return Valid(); }

    template<typename T>
    T Get()
    {
        //Comptime check that T is supported
        static_assert(
            std::is_same<T, i32>() ||
            std::is_same<T, u32>() ||
            std::is_same<T, u16>() ||
            std::is_same<T, u8>() ||
            std::is_same<T, f32>() ||
            std::is_same<T, bool>() ||
            std::is_same<T, string>() ||
            std::is_same<T, std::vector<ObjectHandle>>(), "Unsupported type used by Property::Get<T>()");

        if (!_object)
            THROW_EXCEPTION("Called ::Get() on and invalid property handle!");

        std::lock_guard<std::mutex> lock(*_object->Mutex.get());
        Property& prop = _object->Properties[_index];
        return std::get<T>(prop.Value);
    }

    template<typename T>
    void Set(T value)
    {
        //Comptime check that T is supported
        static_assert(
            std::is_same<T, i32>() ||
            std::is_same<T, u32>() ||
            std::is_same<T, u16>() ||
            std::is_same<T, u8>() ||
            std::is_same<T, f32>() ||
            std::is_same<T, bool>() ||
            std::is_same<T, string>() ||
            std::is_same<T, std::vector<ObjectHandle>>(), "Unsupported type used by Property::Set<T>()");

        if (!_object)
            THROW_EXCEPTION("Called ::Set() on and invalid property handle!");

        std::lock_guard<std::mutex> lock(*_object->Mutex.get());
        Property& prop = _object->Properties[_index];
        prop.Value = value;
    }

    //Special cases of Get<T>() and Set<T>() for object lists. For convenience to avoid using Get<std::vector<ObjectHandle>>()
    std::vector<ObjectHandle>& GetObjectList()
    {
        Property& prop = _object->Properties[_index];
        return std::get<std::vector<ObjectHandle>>(prop.Value);
    }
    void SetObjectList(const std::vector<ObjectHandle>& newList = {})
    {
        Property& prop = _object->Properties[_index];
        prop.Value = newList; //Replaces existing list
    }

private:
    Object* _object;
    u32 _index; //Index of property in Object::Properties
};

//Handle to registry object
class ObjectHandle
{
public:
    ObjectHandle(Object* object) : _object(object)
    {

    }
    u64 UID() const { return _object ? _object->UID : NullUID; }
    bool Valid() const { return _object && _object->UID != NullUID; }
    //Allows checking handle validity with if statements. E.g. if (handle) { /*valid*/ } else { /*invalid*/ }
    explicit operator bool() const { return Valid(); }

    //Todo: Add functions for accessing and modifying object name/type/properties/sub-objects/etc

    PropertyHandle GetProperty(std::string_view name)
    {
        for (size_t i = 0; i < _object->Properties.size(); i++)
            if (_object->Properties[i].Name == name)
                return { _object, (u32)i };

        return { nullptr, std::numeric_limits<u32>::max() };
    }

    PropertyHandle GetOrCreateProperty(std::string_view name)
    {
        if (PropertyHandle handle = GetProperty(name); _object && handle)
        {
            return handle;
        }
        else
        {
            Property& prop = _object->Properties.emplace_back();
            prop.Name = name;
            return { _object, (u32)(_object->Properties.size() - 1) };
        }
    }

    std::vector<PropertyHandle> Properties()
    {
        std::vector<PropertyHandle> properties = {};
        for (u32 i = 0; i < _object->Properties.size(); i++)
            properties.emplace_back(_object, i);

        return properties;
    }

    //Note: Option for manually locking the object mutex. PropertyHandle::Set()//::Get() already do this, but the functions related to object lists don't.
    //The need for this will be removed in the rewrite that'll take place after testing this out on the map editor.
    void Lock()
    {
        _object->Mutex->lock();
    }
    void Unlock()
    {
        _object->Mutex->unlock();
    }

private:
    Object* _object;
};

static const ObjectHandle NullObjectHandle = { nullptr };
static const PropertyHandle NullPropertyHandle = { nullptr, std::numeric_limits<u32>::max() };