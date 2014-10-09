#define PURPLE_PLUGINS

#include "xmpp_muc_presence_plugin.h"

#include <gtk/gtk.h>
#include <libpurple/debug.h>
#include <libpurple/version.h>
#include <pidgin/gtkimhtml.h>
#include <pidgin/gtkplugin.h>
#include <pidgin/gtkutils.h>
#include <pidgin/pidginstock.h>

#define INVALID_TIMER_HANDLE ((guint)-1)
#define TIMEOUT_INTERVAL 1

static PurplePlugin* muc_presence = NULL;
static guint timer_handle = INVALID_TIMER_HANDLE;
static GHashTable* s_presence;

enum {
    STATE_UNKNOWN = 0,
    STATE_AVAILABLE,
    STATE_AWAY,
    STATE_XA,
    STATE_DND,
    STATE_CHAT
};

static gboolean
update_presence_icon()
{
    GList* list;

    timer_handle = INVALID_TIMER_HANDLE;
    purple_debug_info(PLUGIN_ID, "update_presence_icon\n");

    for (list = pidgin_conv_windows_get_list();
         list;
         list = list->next)
    {
        PidginWindow* window = list->data;
        PurpleConversation* conv = window ? pidgin_conv_window_get_active_conversation(window) : NULL;
        PidginConversation* gtkconv = window ? pidgin_conv_window_get_active_gtkconv(window) : NULL;
        PidginChatPane* gtkchat = gtkconv ? gtkconv->u.chat : NULL;
        GtkTreeModel* tm = gtkchat ? gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list)) : NULL;
        GtkListStore* ls = tm ? GTK_LIST_STORE(tm) : NULL;

        if (conv != NULL && tm != NULL && ls != NULL)
        {
            GtkTreeIter iter;

            if (gtk_tree_model_get_iter_first(tm, &iter))
            {
                do
                {
                    gchar* alias = NULL;
                    gchar* name = NULL;
                    char* jid = NULL;
                    int state = STATE_AVAILABLE;
                    const char* stock = NULL;

                    gtk_tree_model_get(
                      tm,
                      &iter,
                      CHAT_USERS_ALIAS_COLUMN, &alias,
                      CHAT_USERS_NAME_COLUMN, &name,
                      -1);

                    jid = g_strdup_printf("%s/%s", conv->name, alias ? alias : name);
                    state = (int)g_hash_table_lookup(s_presence, jid);
                    g_free(jid);

                    if (state != STATE_UNKNOWN)
                    {
                        switch (state)
                        {
                        case STATE_AWAY: stock = PIDGIN_STOCK_STATUS_AWAY; break;
                        case STATE_CHAT: stock = PIDGIN_STOCK_STATUS_CHAT; break;
                        case STATE_XA: stock = PIDGIN_STOCK_STATUS_XA; break;
                        case STATE_DND: stock = PIDGIN_STOCK_STATUS_BUSY; break;
                        default: stock = PIDGIN_STOCK_STATUS_AVAILABLE; break;
                        }

                        gtk_list_store_set(ls, &iter, CHAT_USERS_ICON_STOCK_COLUMN, stock, -1);
                    }
                }
                while (gtk_tree_model_iter_next(tm, &iter));
            }
        }
    }

    return FALSE;
}

static void
register_timeout_update_presence_icon(guint interval)
{
    if (timer_handle != INVALID_TIMER_HANDLE)
    {
        purple_timeout_remove(timer_handle);
    }

    timer_handle = purple_timeout_add_seconds(
        interval, (GSourceFunc)update_presence_icon, NULL);
}

static void
handle_conversation_displayed(PurpleConversation* conv)
{
    purple_debug_info(PLUGIN_ID, "handle_conversation_displayed %p\n", conv);
    register_timeout_update_presence_icon(TIMEOUT_INTERVAL);
}

static gboolean
handle_jabber_receiving_presence(
    PurpleConnection* pc,
    const char* type,
    const char* from,
    xmlnode* presence)
{
    xmlnode* show = NULL;
    char* showText = NULL;
    int state = STATE_AVAILABLE;

    purple_debug_info(PLUGIN_ID, "handle_jabber_receiving_presence %p %s %s %p\n", pc, type, from, presence);

    show = xmlnode_get_child(presence, "show");
    showText = show ? xmlnode_get_data(show) : NULL;

    purple_debug_info(PLUGIN_ID, "  show %s\n", showText);

    if (showText)
    {
        if (strcmp(showText, "away") == 0)
        {
            state = STATE_AWAY;
        }
        else if (strcmp(showText, "dnd") == 0)
        {
            state = STATE_DND;
        }
        else if (strcmp(showText, "xa") == 0)
        {
            state = STATE_XA;
        }
        else if (strcmp(showText, "chat") == 0)
        {
            state = STATE_CHAT;
        }
    }

    g_hash_table_replace(s_presence, (gpointer)g_strdup(from), (gpointer)state);

    register_timeout_update_presence_icon(TIMEOUT_INTERVAL);

    // don't stop signal processing
    return FALSE;
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
    PurplePlugin* jabber = NULL;

    s_presence = g_hash_table_new_full(
        g_str_hash,
        g_str_equal,
        (GDestroyNotify)g_free,
        (GDestroyNotify)NULL);

    muc_presence = plugin;

    purple_signal_connect(
        pidgin_conversations_get_handle(),
        "conversation-displayed",
        plugin, PURPLE_CALLBACK(handle_conversation_displayed), NULL);


    if ((jabber = purple_find_prpl("prpl-jabber")) != NULL)
    {
        purple_signal_connect(
            jabber,
            "jabber-receiving-presence",
            plugin, PURPLE_CALLBACK(handle_jabber_receiving_presence), NULL);
    }

    return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
    g_hash_table_destroy(s_presence);
    return TRUE;
}

static PurplePluginInfo info = {
    PURPLE_PLUGIN_MAGIC,
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_STANDARD,
    PIDGIN_PLUGIN_TYPE,
    0,
    NULL,
    PURPLE_PRIORITY_DEFAULT,

    PLUGIN_ID,
    "XMPP MUC Presence plugin",
    "0.1",

    "MUC Presence plugin",
    "Show status icon in catroom",
    "Takashi Matsuda <matsu@users.sf.net>",
    "https://github.com/tmatz/pidgin-xmpp-muc-presence-plugin",

    plugin_load,
    plugin_unload,
    NULL, /* destroy */

    NULL, /* ui_info */
    NULL, /* extra_info */
    NULL, /* prefs_into */
    NULL, /* plugin_actions */

    /* padding */
    NULL,
    NULL,
    NULL,
    NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(xmpp_muc_presence_plugin, init_plugin, info)
