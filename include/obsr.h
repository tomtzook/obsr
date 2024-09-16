#pragma once

#include <string>
#include <functional>
#include <chrono>
#include <ostream>

#include "obsr_types.h"

namespace obsr {

/**
 * On behaviour of objects:
 *
 * Objects act as folders, capable of storing multiple child objects (sub-folders) and entries (files).
 * Objects are described by paths, and can be used by acquiring an handle for them. The path is made up of
 * parent objects and the name of the particular object.
 *
 * There is a single root object from which all objects are derived.
 */

/**
 * On behaviour of entries
 *
 * Entries act as a storage for a single obsr::value. It is described by an handle and can be accessed by acquiring
 * an handle to it. Each entry is contained within an object. The path is made up of
 * parent objects and the name of the particular entry.
 */

/**
 * On network behaviour
 *
 * Once network services are started, obsr will attempt to establish and maintain connects with other
 * obsr online programs (obsr programs with network services running). This allows for the synchronization of
 * data between the programs. This includes objects, entries and values as exist and are set/created/deleted in each
 * program.
 *
 * It is important to understand the values, entries and objects may be changed behind the scene due to changes
 * done in other programs. One may register listeners to track such changes and act accordingly.
 *
 * Because multiple programs may manipulate the same objects/entries, there can be an unexpected behaviour due to
 * the lack of strong synchronization between the programs. As such, it is recommended that for each entry, only one
 * program will write to it while other programs read from it.
 */

/**
 * Gets the time registered in the local obsr clock. This clock may be synced to the local system clock
 * or to a remote obsr server.
 *
 * @return time in milliseconds from clock.
 */
std::chrono::milliseconds time();

/**
 * Gets the root object. All objects and entries are derived from this root.
 * It has no parent and cannot be deleted.
 *
 * @return root object
 */
object get_root();

/**
 * Gets an object based on an absolute path. If this object doesn't exist, it will be created, otherwise the exiting
 * object is returned. If the parents of this object don't exist, they will be created.
 *
 * @param path path to the object, formatted as "/path/to/object"
 * @return object at path
 */
object get_object(std::string_view path);

/**
 * Gets an entry based on an absolute path. If this entry doesn't exist, it is created with an empty value.
 * If the parents of this entry don't exist, they will be created.
 *
 * @param path path to the entry, formatted as "/path/to/entry"
 * @return entry at path
 */
entry get_entry(std::string_view path);

/**
 * Gets a child object of this object with a specific name. If such an object does not exist, it will be created.
 *
 * At the moment, calling this function with obj being deleted, is undefined.
 *
 * @param obj parent object
 * @param name name of child object.
 * @return child object with name.
 */
object get_child(object obj, std::string_view name);

/**
 * Gets the entry in an object with a specific name. If such entry does not exist, it is created with an empty value.
 *
 * At the moment, calling this function with obj being deleted, is undefined.
 *
 * @param obj parent object
 * @param name name of the entry.
 * @return entry at object with name.
 */
entry get_entry(object obj, std::string_view name);

/**
 * Gets the parent object of a given object.
 * If the given object is root, an exception is raised.
 *
 * At the moment, calling this function with obj being deleted, is undefined.
 *
 * @param obj object
 * @return parent of obj
 */
object get_parent_for_object(object obj);

/**
 * Gets the parent object of a given entry.
 * If the entry does not exist, an exception is thrown.
 *
 * At the moment, calling this function with obj being deleted, is undefined.
 *
 * @param entry entry
 * @return parent of entry
 */
object get_parent_for_entry(entry entry);

/**
 * Deletes an object, with all of its children and entries.
 *
 * After this call, the given object handle should not be used again.
 *
 * This call generates a deleted event.
 *
 * @param obj object to delete.
 */
void delete_object(object obj);

/**
 * Deletes a specific entry.
 *
 * After this call, the given entry handle should not be used again.
 *
 * This call generates a deleted event.
 *
 * @param entry entry to delete.
 */
void delete_entry(entry entry);

/**
 * Gets the status and flags associated with an entry.
 *
 * If the entry does not exist, obsr::entry_not_exists is returned. Otherwise a bitmask of obsr::entry_flag
 * is returned.
 *
 * @param entry entry to check
 * @return flags associated with entry.
 */
uint32_t probe(entry entry);

/**
 * Gets the value associated with a given entry.
 *
 * If the entry doesn't exist, an exception is thrown.
 *
 * @param entry entry
 * @return value associated with entry.
 */
obsr::value get_value(entry entry);

/**
 * Sets the value associated with a given entry.
 *
 * If the entry does not exist, it is created.
 *
 * This call generates a value_changed event. If the entry did not have a value prior to this call, a created event
 * is generated.
 *
 * @param entry entry
 * @param value value to set
 */
void set_value(entry entry, const obsr::value& value);

/**
 * Clears the value associated with a given entry. Effectively setting the value to an empty value.
 *
 * If the entry does not exist, it is created.
 *
 * This call generates a value_changed event. If the entry did not have a value prior to this call, a created event
 * is generated.
 *
 * @param entry entry
 */
void clear_value(entry entry);

/**
 * Listens to events generated for an object and all of its children and entries. Only events generated
 * after this calls will be received by the listener.
 *
 * @param obj object to listen to
 * @param callback listener callback
 * @return listener handle
 */
listener listen_object(object obj, const listener_callback&& callback);

/**
 * Listens to events generated for an entry. Only events generated
 * after this calls will be received by the listener.
 *
 * @param entry entry to listen to
 * @param callback listener callback
 * @return listener handle
 */
listener listen_entry(entry entry, const listener_callback&& callback);

/**
 * Deletes a listener associated with the given handle. Events will not be generated for the callback after this
 * call.
 *
 * @param listener handle associated to the listener to remove.
 */
void delete_listener(listener listener);

/**
 * Starts network services as a server node, allowing remote obsr client nodes to connect and synchronize
 * data among each other.
 *
 * @param bind_port port to bind server to.
 */
void start_server(uint16_t bind_port);

/**
 * Starts network services as a client node, automatically connecting to a remote server (if it is online)
 * and synchronizing data with it and other clients connected to the server.
 *
 * @param address server ip address
 * @param server_port server port
 */
void start_client(std::string_view address, uint16_t server_port);

/**
 * Stops any active network services.
 */
void stop_network();

}

// these are textual and should not be used to store in file or send
std::ostream& operator<<(std::ostream& os, obsr::value_type type);
std::ostream& operator<<(std::ostream& os, const obsr::value& value);
std::ostream& operator<<(std::ostream& os, obsr::event_type type);
std::ostream& operator<<(std::ostream& os, const obsr::event& event);
