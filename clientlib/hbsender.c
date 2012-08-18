/**
 * @file
 * @brief Implements the @ref HbSender class - for sending heartbeats.
 * @details We are told what addresses to send to, and how often to send them.
 *
 * @author &copy; 2011 - Alan Robertson <alanr@unix.sh>
 * @n
 * Licensed under the GNU Lesser General Public License (LGPL) version 3 or any later version at your option,
 * excluding the provision allowing for relicensing under the GPL at your option.
 */
#include <memory.h>
#include <glib.h>
#include <frameset.h>
#include <frame.h>
#include <hbsender.h>
/**
 */
FSTATIC void _hbsender_notify_function(gpointer data);
FSTATIC void _hbsender_finalize(HbSender * self);
FSTATIC void _hbsender_ref(HbSender * self);
FSTATIC void _hbsender_unref(HbSender * self);
FSTATIC void _hbsender_addlist(HbSender* self);
FSTATIC void _hbsender_dellist(HbSender* self);
FSTATIC void _hbsender_sendheartbeat(HbSender* self);
FSTATIC gboolean _hbsender_gsourcefunc(gpointer);

guint64 proj_get_real_time(void); 	///@todo - make this a real global function

///@defgroup HbSender HbSender class.
/// Class for heartbeat Senders - We send heartbeats to the chosen few
///@{
///@ingroup C_Classes

static GSList*	_hb_senders = NULL;
static gint	_hb_sender_count = 0;

#define	ONESEC	1000000

/// Add an HbSender to our global list of HbSenders
FSTATIC void
_hbsender_addlist(HbSender* self)	///<[in]The sender to add
{
	_hb_senders = g_slist_prepend(_hb_senders, self);
	_hb_sender_count += 1;
}

/// Remove an HbSender from our global list of HbSenders
FSTATIC void
_hbsender_dellist(HbSender* self)	///<[in]The sender to remove from our list
{
	if (g_slist_find(_hb_senders, self) != NULL) {
		_hb_senders = g_slist_remove(_hb_senders, self);
		_hb_sender_count -= 1;
		return;
	}
	g_warn_if_reached();
}

/// A GSourceFunc to be used with g_timeout_add_seconds()
FSTATIC gboolean
_hbsender_gsourcefunc(gpointer gself) ///<[in/out] Pointer to 'self'
{
	HbSender* self = CASTTOCLASS(HbSender, gself);
	_hbsender_sendheartbeat(self);
	return TRUE;
}


/// Increment the reference count by one.
FSTATIC void
_hbsender_ref(HbSender* self)	///<[in/out] Object to increment reference count for
{
	self->_refcount += 1;
}

/// Decrement the reference count by one - possibly freeing up the object.
FSTATIC void
_hbsender_unref(HbSender* self)	///<[in/out] Object to decrement reference count for
{
	g_return_if_fail(self->_refcount > 0);
	self->_refcount -= 1;
	if (self->_refcount == 0) {
		_hbsender_dellist(self);
		self->_finalize(self);
		self = NULL;
	}
}
// Callback function from the GSource world - notifying us when we're getting shut down from their end
FSTATIC void
_hbsender_notify_function(gpointer data)
{
	HbSender* self = CASTTOCLASS(HbSender, data);
	self->timeout_source = 0;
}

/// Finalize an HbSender
FSTATIC void
_hbsender_finalize(HbSender * self) ///<[in/out] Sender to finalize
{
	if (self->_sendaddr) {
		self->_sendaddr->baseclass.unref(self->_sendaddr);
		self->_sendaddr = NULL;
	}
	if (self->timeout_source != 0) {
		g_source_remove(self->timeout_source);
	}
	memset(self, 0x00, sizeof(*self));
	FREECLASSOBJ(self);
}


/// Construct a new HbSender - setting up timeout data structures for it.
/// This can be used directly or by derived classes.
HbSender*
hbsender_new(NetAddr* sendaddr,		///<[in] Address to send to
	     NetGSource* outmethod,	///<[in] Mechanism for sending packets
	     guint  interval,		///<[in] How often to send, in seconds
	     gsize objsize)		///<[in] size of HbSender structure (0 for sizeof(HbSender))
{
	HbSender * newsender;
	if (objsize < sizeof(HbSender)) {
		objsize = sizeof(HbSender);
	}
	newsender = MALLOCCLASS(HbSender, objsize);
	if (newsender != NULL) {
		newsender->_sendaddr = sendaddr;
		sendaddr->baseclass.ref(sendaddr);
		newsender->_refcount = 1;
		newsender->ref = _hbsender_ref;
		newsender->_outmethod = outmethod;
		newsender->unref = _hbsender_unref;
		newsender->_finalize = _hbsender_finalize;
		newsender->_expected_interval = interval;
		if (interval < 500000) {
			interval = 1000000;
		}
		newsender->timeout_source = g_timeout_add_seconds_full
					    (G_PRIORITY_HIGH, (interval/1000000), _hbsender_gsourcefunc
					,    newsender, _hbsender_notify_function);
		g_message("Sender %p timeout source is: %d, interval is %d", newsender
		,	  newsender->timeout_source, interval);
		_hbsender_addlist(newsender);
		// Avoid Martian packets - don't send the first one right away...
		//	_hbsender_sendheartbeat(newsender);
	}
	return newsender;
}


/// Stop sending heartbeats to a particular address
void
hbsender_stopsend(NetAddr* sendaddr)///<[in/out] Sender to remove from list
{
	GSList*		obj;
	for (obj = _hb_senders; obj != NULL; obj=obj->next) {
		HbSender* sender = CASTTOCLASS(HbSender, obj->data);
		if (sendaddr->equal(sendaddr, sender->_sendaddr)) {
			sender->unref(sender);
			return;
		}
	}
}
FSTATIC void
_hbsender_sendheartbeat(HbSender* self)
{
	FrameSet*	heartbeat = frameset_new(FRAMESETTYPE_HEARTBEAT);
	//g_debug("Sending a heartbeat...");
	self->_outmethod->sendaframeset(self->_outmethod, self->_sendaddr, heartbeat);
	heartbeat->unref(heartbeat); heartbeat = NULL;
}
void
hbsender_stopallsenders(void)
{
	while (_hb_senders) {
		HbSender* sender = CASTTOCLASS(HbSender, _hb_senders->data);
		sender->unref(sender);
	}
}
///@}
