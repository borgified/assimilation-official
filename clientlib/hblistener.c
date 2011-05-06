/**
 * @file
 * @brief Implements the @ref HbListener class - for listening to heartbeats.
 * @details We are told what addresses to listen for, what ones to stop listening for at run time
 * and time out both warning times, and fatal (dead) times.
 *
 * @author &copy; 2011 - Alan Robertson <alanr@unix.sh>
 * @n
 * Licensed under the GNU Lesser General Public License (LGPL) version 3 or any later version at your option,
 * excluding the provision allowing for relicensing under the GPL at your option.
 */
#include <memory.h>
#include <glib.h>
#include <frame.h>
#include <hblistener.h>
/**
 */
FSTATIC void _hblistener_finalize(HbListener * self);
FSTATIC void _hblistener_ref(HbListener * self);
FSTATIC void _hblistener_unref(HbListener * self);
FSTATIC void _hblistener_addlist(HbListener* self);
FSTATIC void _hblistener_dellist(HbListener* self);
FSTATIC void _hblistener_checktimeouts(gboolean urgent);
FSTATIC void _hblistener_hbarrived(FrameSet* fs, NetAddr* srcaddr);
FSTATIC void _hblistener_addlist(HbListener* self);
FSTATIC void _hblistener_dellist(HbListener* self);

guint64 proj_get_real_time(void); 	///@todo - make this a real global function

///@defgroup HbListener HbListener class.
/// Class for heartbeat Listeners - We listen for heartbeats and time out those which are late.
///@{
///@ingroup C_Classes

static GList*	_hb_listeners = NULL;
static gint	_hb_listener_count = 0;
static guint64	_hb_listener_lastcheck = 0;
static void	(*_hblistener_deadcallback)(HbListener* who) = NULL;
static void 	(*_hblistener_warncallback)(HbListener* who, guint64 howlate) = NULL;
static void 	(*_hblistener_comealivecallback)(HbListener* who, guint64 howlate) = NULL;
static void	(*_hblistener_martiancallback)(const NetAddr* who) = NULL;

#define	ONESEC	1000000
/// Add an HbListener to our global list of HBListeners
FSTATIC void
_hblistener_addlist(HbListener* self)	/// The listener to add
{
	_hb_listeners = g_list_prepend(_hb_listeners, self);
	_hb_listener_count += 1;
	self->ref(self);
}

/// Remove an HbListener from our global list of HBListeners
FSTATIC void
_hblistener_dellist(HbListener* self)	/// The listener to remove from our list
{
	if (g_list_find(_hb_listeners, self) != NULL) {
		_hb_listeners = g_list_remove(_hb_listeners, self);
		_hb_listener_count -= 1;
		self->unref(self);
		return;
	}
	g_warn_if_reached();
}

/// Function called when it's time to see if anyone timed out...
FSTATIC void
_hblistener_checktimeouts(gboolean urgent)
{
	guint64		now = proj_get_real_time();
	GList*		obj;
	if (!urgent && (now - _hb_listener_lastcheck) < ONESEC) {
		return;
	}
	_hb_listener_lastcheck = now;

	for (obj = _hb_listeners; obj != NULL; obj=obj->next) {
		HbListener* listener = CASTTOCLASS(HbListener, obj->data);
		if (now > listener->nexttime && listener->status == HbPacketsBeingReceived) {
			if (_hblistener_deadcallback) {
				_hblistener_deadcallback(listener);
			}
			g_warning("our node looks dead from here...");
			listener->status = HbPacketsTimedOut;
		}
	}
}
/// Function called when a heartbeat @ref Frame arrived from the given @ref NetAddr
FSTATIC void
_hblistener_hbarrived(FrameSet* fs, NetAddr* srcaddr)
{
	GList*		obj;
	guint64		now = proj_get_real_time();
	for (obj = _hb_listeners; obj != NULL; obj=obj->next) {
		HbListener* listener = CASTTOCLASS(HbListener, obj->data);
		if (srcaddr->equal(srcaddr, listener->listenaddr)) {
			///@todo ADD CODE TO PROCESS PACKET, not just observe that it arrived??
			/// - maybe another callback?
			if (listener->status == HbPacketsTimedOut) {
				guint64 howlate = now - listener->nexttime;
				g_message("Our node is now back alive!");
				if (_hblistener_comealivecallback) {
					_hblistener_comealivecallback(listener, howlate);
				}
				listener->status = HbPacketsBeingReceived;
			} else if (now > listener->warntime) {
				guint64 howlate = now - listener->warntime;
				howlate /= 1000;
				g_warning("our node is %lums late in sending heartbeat..."
				,	howlate);
				if (_hblistener_warncallback) {
					_hblistener_warncallback(listener, howlate);
				}
			}
			listener->nexttime = now + listener->_expected_interval;
			listener->warntime = now + listener->_warn_interval;
			return;
		}
	}
	if (_hblistener_martiancallback) {
		_hblistener_martiancallback(srcaddr);
	}
	g_warn_if_reached();
}

/// Increment the reference count by one.
FSTATIC void
_hblistener_ref(HbListener* self)
{
	self->_refcount += 1;
}

/// Decrement the reference count by one - possibly freeing up the object.
FSTATIC void
_hblistener_unref(HbListener* self)
{
	g_return_if_fail(self->_refcount > 0);
	self->_refcount -= 1;
	if (self->_refcount == 0) {
		self->_finalize(self);
		self = NULL;
	}
}

/// Finalize an HbListener
FSTATIC void
_hblistener_finalize(HbListener * self) ///< Listener to finalize
{
	self->listenaddr->unref(self->listenaddr);
	// self->listenaddr = NULL;
	memset(self, 0x00, sizeof(*self));
	FREECLASSOBJ(self);
}


/// Construct a new HbListener - setting up GSource and timeout data structures for it.
/// This can be used directly or by derived classes.
///@todo Create Gsource for packet reception, attach to context, write dispatch code,
///@todo Create scan tag - create GSource, attach to context, write dispatch code
/// to call _hblistener_hbarrived() - ensuring we don't call it any more often than every second or so.
HbListener*
hblistener_new(NetAddr*	listenaddr,	///<[in] Address to listen to
	       gsize objsize)	///<[in] size of HbListener structure (or zero for sizeof(HbListener))
{
	HbListener * newlistener;
	if (objsize < sizeof(HbListener)) {
		objsize = sizeof(HbListener);
	}
	newlistener = MALLOCCLASS(HbListener, objsize);
	if (newlistener != NULL) {
		newlistener->listenaddr = listenaddr;
		listenaddr->ref(listenaddr);
		newlistener->_refcount = 1;
		newlistener->ref = _hblistener_ref;
		newlistener->unref = _hblistener_unref;
		newlistener->_finalize = _hblistener_finalize;
		newlistener->_expected_interval = DEFAULT_DEADTIME * 1000000;
		newlistener->_warn_interval = newlistener->_expected_interval / 4;
		newlistener->nexttime = proj_get_real_time() + newlistener->_expected_interval;
		newlistener->warntime = proj_get_real_time() + newlistener->_warn_interval;
		newlistener->status = HbPacketsBeingReceived;
		_hblistener_addlist(newlistener);
	}
	return newlistener;
}
/// Stop expecting (listening for) heartbeats from a particular address
void
hblistener_unlisten(NetAddr* unlistenaddr)
{
	GList*		obj;
	for (obj = _hb_listeners; obj != NULL; obj=obj->next) {
		HbListener* listener = CASTTOCLASS(HbListener, obj->data);
		if (unlistenaddr->equal(unlistenaddr, listener->listenaddr)) {
			_hblistener_dellist(listener);
			return;
		}
	}
	g_warning("Attempt to unlisten an unregistered address");
}
/// Call to set a callback to be called when a node apparently dies
void
hblistener_set_deadtime_callback(void (*callback)(HbListener* who))
{
	_hblistener_deadcallback = callback;
}
/// Call to set a callback to be called when a node passes warntime before heartbeating again
void
hblistener_set_warntime_callback(void (*callback)(HbListener* who, guint64 howlate))
{
	_hblistener_warncallback = callback;
}
/// Call to set a callback to be called when a node passes deadtime but heartbeats again
void
hblistener_set_comealive_callback(void (*callback)(HbListener* who, guint64 howlate))
{
	_hblistener_comealivecallback = callback;
}
/// Call to set a callback to be called when an unrecognized node sends us a heartbeat
void
hblistener_set_martian_callback(void (*callback)(const NetAddr* who))
{
	_hblistener_martiancallback = callback;
}
