/////////////////////////////////////////////////////////////////////////////
/// @file iclient_persistence.h
/// Declaration of MQTT iclient_persistence interface
/// @date May 1, 2013
/// @author Frank Pagliughi
/////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
 * Copyright (c) 2013-2016 Frank Pagliughi <fpagliughi@mindspring.com>
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Frank Pagliughi - initial implementation and documentation
 *******************************************************************************/

#ifndef __mqtt_iclient_persistence_h
#define __mqtt_iclient_persistence_h

#include "MQTTAsync.h"
#include "mqtt/types.h"
#include "mqtt/buffer_view.h"
#include "mqtt/string_collection.h"
#include <vector>

namespace mqtt {

/**
 * Allocate memory for use with user persistence.
 *
 * @param n The number of bytes for the buffer.
 * @return A pointer to the allocated memory
 */
inline char* persistence_malloc(size_t n) {
	return static_cast<char*>(MQTTAsync_malloc(n));
}

/**
 * Frees memory allocated with @ref persistence_malloc
 * @param p Pointer to a buffer obtained by persistence_malloc.
 */
inline void persistence_free(char* p) {
	MQTTAsync_free(p);
}

/////////////////////////////////////////////////////////////////////////////

/**
 * Represents a persistent data store, used to store outbound and inbound
 * messages while they are in flight, enabling delivery to the QoS
 * specified. You can specify an implementation of this interface using
 * client::client(string, string, iclient_persistence), which the
 * client will use to persist QoS 1 and 2 messages.
 *
 * If the methods defined throw the MqttPersistenceException then the state
 * of the data persisted should remain as prior to the method being called.
 * For example, if put(string, persistable) throws an exception at any
 * point then the data will be assumed to not be in the persistent store.
 * Similarly if remove(string) throws an exception then the data will be
 * assumed to still be held in the persistent store.
 *
 * It is up to the persistence interface to log any exceptions or error
 * information which may be required when diagnosing a persistence failure.
 */
class iclient_persistence
{
	friend class async_client;
	friend class mock_persistence;

	/** Callbacks from the C library */
	static int persistence_open(void** handle, const char* clientID, const char* serverURI, void* context);
	static int persistence_close(void* handle);
	static int persistence_put(void* handle, char* key, int bufcount, char* buffers[], int buflens[]);
	static int persistence_get(void* handle, char* key, char** buffer, int* buflen);
	static int persistence_remove(void* handle, char* key);
	static int persistence_keys(void* handle, char*** keys, int* nkeys);
	static int persistence_clear(void* handle);
	static int persistence_containskey(void* handle, char* key);

public:
	/** Smart/shared pointer to an object of this class. */
	using ptr_t = std::shared_ptr<iclient_persistence>;
	/** Smart/shared pointer to a const object of this class. */
	using const_ptr_t = std::shared_ptr<const iclient_persistence>;

	/**
	 * Virtual destructor.
	 */
	virtual ~iclient_persistence() {}
	/**
	 * Initialize the persistent store.
	 * This uses the client ID and server name to create a unique location
	 * for the data store.
	 * @param clientId The identifier string for the client.
	 * @param serverURI The server to which the client is connected.
	 */
	virtual void open(const string& clientId, const string& serverURI) =0;
	/**
	 * Close the persistent store that was previously opened.
	 */
	virtual void close() =0;
	/**
	 * Clears persistence, so that it no longer contains any persisted data.
	 */
	virtual void clear() =0;
	/**
	 * Returns whether or not data is persisted using the specified key.
	 * @param key The key to find
	 * @return @em true if the key exists, @em false if not.
	 */
	virtual bool contains_key(const string& key) =0;
	/**
	 * Returns a collection of keys in this persistent data store.
	 * @return A collection of strings representing the keys in the store.
	 */
	virtual const string_collection& keys() const =0;
	/**
	 * Puts the specified data into the persistent store.
	 * @param key The key.
	 * @param bufs The data to store
	 */
	virtual void put(const string& key, const std::vector<string_view>& bufs) =0;
	/**
	 * Gets the specified data out of the persistent store.
	 * @param key The key
	 * @return A const view of the data associated with the key.
	 */
	virtual string_view get(const string& key) const =0;
	/**
	 * Remove the data for the specified key.
	 * @param key The key
	 */
	virtual void remove(const string& key) =0;
};

/** Smart/shared pointer to a persistence client */
using iclient_persistence_ptr = iclient_persistence::ptr_t;

/** Smart/shared pointer to a persistence client */
using const_iclient_persistence_ptr = iclient_persistence::const_ptr_t;

/////////////////////////////////////////////////////////////////////////////

/**
 * Interface for objects to encode and decode data going to and from the
 * persistence store.
 *
 * This is typically used to encrypt the data before writing to
 * persistence, and then decrypt it when reading it back from persistence.
 *
 * For optimized performance, the application can perform encoding in-place
 * with each of the supplied buffers, if the resulting data fits. But, if
 * not, it's left to the application to do its own memory management with
 * @ref persistence_malloc() and @ref persistence_free().
 */
class ipersistence_encoder
{
	friend class async_client;

	/** Callbacks from the C library */
	static int before_write(void* context, int bufcount, char* buffers[], int buflens[]);
	static int after_read(void* context, char** buffer, int* buflen);

public:
	/**
	 * Virtual destructor.
	 */
	virtual ~ipersistence_encoder() {}
	/**
	 * Callback to let the application encode data before writing it to
	 * persistence.
	 *
	 * This is called just prior to writing the data to persistence.
	 *
	 * If the encoded data fits into each of the supplied buffers, the
	 * encoding can be done in place. If a buffer needs to grow, the
	 * application can call @ref persistence_malloc() to get a new buffer,
	 * and update the pointer. It then needs to deallocate the old buffer.
	 * In either case it should update the new size of the buffer.
	 *
	 * @param nbuf The number of buffers to encode.
	 * @param bufs The addresses of the data buffers to be encoded.
	 * @param lens The length of each buffer.
	 */
	virtual void encode(size_t nbuf, char* bufs[], size_t lens[]) =0;
	/**
	 * Callback to let the application decode data after it is retrieved
	 * from persistence.
	 *
	 * If the decoded data fits into the supplied buffer, the decoding can
	 * be done in place. If the buffer needs to grow, the application can
	 * call @ref persistence_malloc() to get a new buffer, and update the
	 * pointer. It then needs to deallocate the old buffer. In either case
	 * it should update the new size of the buffer.
	 *
	 * @param pbuf Pointer to the data buffer to decoded.
	 * @param len Pointer to the length of the buffer.
	 */
	virtual void decode(char** pbuf, size_t* len) =0;
};


/////////////////////////////////////////////////////////////////////////////
// end namespace mqtt
}

#endif		// __mqtt_iclient_persistence_h

