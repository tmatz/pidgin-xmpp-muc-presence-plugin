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
#define TIMEOUT_INTERVAL 2
#define PROCESSED_KEY "xmpp_muc_presence_plugin_processed"
#define PROCESSED_MARK GINT_TO_POINTER(1)
#define ICON_STOCK_NULL GINT_TO_POINTER(-1)
#define PROTOCOL_JABBER "prpl-jabber"

#define PREF_PREFIX "/plugins/gtk/" PLUGIN_ID
#define PREF_SHOW_PRESENCE PREF_PREFIX "/show_presence"

enum {
    STATE_UNKNOWN = 0,
    STATE_AVAILABLE,
    STATE_AWAY,
    STATE_XA,
    STATE_DND,
    STATE_CHAT
};

static PurplePlugin* s_muc_presence = NULL;
static GHashTable* s_presence = NULL;
static GHashTable* s_original_stock = NULL;

static gboolean is_jabber(PidginConversation* gtkconv)
{
    PurpleConversation *conv = gtkconv ? gtkconv->active_conv : NULL;
    PurpleAccount *acc = conv ? conv->account : NULL;

    if (acc &&
        strcmp(PROTOCOL_JABBER, purple_account_get_protocol_id(acc)) == 0)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static gboolean is_processed(PidginConversation* gtkconv)
{
    PurpleConversation* conv = gtkconv ? gtkconv->active_conv : NULL;
    return purple_conversation_get_data(conv, PROCESSED_KEY) == PROCESSED_MARK;
}

static void set_processed(PidginConversation* gtkconv)
{
    PurpleConversation* conv = gtkconv ? gtkconv->active_conv : NULL;
    purple_conversation_set_data(conv, PROCESSED_KEY, PROCESSED_MARK);
}

static void memory_original_stock_icon(const char* jid, const char* stock)
{
    g_hash_table_replace(
        s_original_stock,
        (gpointer)g_strdup(jid),
        (gpointer)(stock ? stock : ICON_STOCK_NULL));
}

static gboolean lookup_original_stock_icon(const char* jid, const char** o_stock)
{
    const char* stock = g_hash_table_lookup(s_original_stock, jid);
    if (o_stock)
    {
        *o_stock = (stock && stock != ICON_STOCK_NULL) ? stock : NULL;
    }
    return stock != NULL;
}

static void
restore_original_stock_icon(PidginConversation* gtkconv)
{
    PurpleConversation* conv = gtkconv ? gtkconv->active_conv : NULL;
    PurpleConvChat* chat = conv ? PURPLE_CONV_CHAT(conv) : NULL;
    PidginChatPane* gtkchat = gtkconv ? gtkconv->u.chat : NULL;
    GtkTreeModel* tm = gtkchat ? gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list)) : NULL;
    GtkListStore* ls = tm ? GTK_LIST_STORE(tm) : NULL;

    if (!is_jabber(gtkconv) || !is_processed(gtkconv))
    {
        return;
    }

    if (conv && chat && tm && ls)
    {
        GtkTreeIter iter;

        if (gtk_tree_model_get_iter_first(tm, &iter))
        {
            do
            {
                gchar* alias = NULL;
                gchar* name = NULL;
                const char* currStock = NULL;
                const char* origStock = NULL;

                gtk_tree_model_get(
                  tm,
                  &iter,
                  CHAT_USERS_ALIAS_COLUMN, &alias,
                  CHAT_USERS_NAME_COLUMN, &name,
                  CHAT_USERS_ICON_STOCK_COLUMN, &currStock,
                  -1);

                {
                    char* jid = g_strdup_printf("%s/%s", conv->name, alias ? alias : name);

                    if (lookup_original_stock_icon(jid, &origStock))
                    {
                        if (currStock != origStock)
                        {
                            gtk_list_store_set(
                                ls,
                                &iter,
                                CHAT_USERS_ICON_STOCK_COLUMN, origStock,
                                -1);
                        }
                    }

                    g_free(jid);
                }
            }
            while (gtk_tree_model_iter_next(tm, &iter));
        }
    }
}

static void
set_presence_stock_icon(PidginConversation* gtkconv)
{
    PurpleConversation* conv = gtkconv ? gtkconv->active_conv : NULL;
    PurpleConvChat* chat = conv ? PURPLE_CONV_CHAT(conv) : NULL;
    PidginChatPane* gtkchat = gtkconv ? gtkconv->u.chat : NULL;
    GtkTreeModel* tm = gtkchat ? gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list)) : NULL;
    GtkListStore* ls = tm ? GTK_LIST_STORE(tm) : NULL;

    if (!is_jabber(gtkconv))
    {
        return;
    }

    if (gtkconv && conv && chat && tm && ls)
    {
        GtkTreeIter iter;
        gboolean memory = FALSE;

        if (!is_processed(gtkconv))
        {
            memory = TRUE; 
            set_processed(gtkconv);
        }

        if (gtk_tree_model_get_iter_first(tm, &iter))
        {
            do
            {
                gchar* alias = NULL;
                gchar* name = NULL;
                int state = STATE_AVAILABLE;
                const char* currStock = NULL;
                const char* stock = NULL;

                gtk_tree_model_get(
                  tm,
                  &iter,
                  CHAT_USERS_ALIAS_COLUMN, &alias,
                  CHAT_USERS_NAME_COLUMN, &name,
                  CHAT_USERS_ICON_STOCK_COLUMN, &currStock,
                  -1);

                {
                    char* jid = g_strdup_printf("%s/%s", conv->name, alias ? alias : name);

                    state = (int)g_hash_table_lookup(s_presence, jid);
                    if (memory || !lookup_original_stock_icon(jid, NULL))
                    {
                        memory_original_stock_icon(jid, currStock);
                    }

                    g_free(jid);
                }

                if (!purple_conv_chat_is_user_ignored(chat, name))
                {
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

                        if (!currStock || strcmp(currStock, stock) != 0)
                        {
                            gtk_list_store_set(ls, &iter, CHAT_USERS_ICON_STOCK_COLUMN, stock, -1);
                        }
                    }
                }
            }
            while (gtk_tree_model_iter_next(tm, &iter));
        }
    }
}

static void
update_stock_icon(PidginConversation* gtkconv)
{
    if (purple_prefs_get_bool(PREF_SHOW_PRESENCE))
    {
        set_presence_stock_icon(gtkconv);
    }
    else
    {
        restore_original_stock_icon(gtkconv);
    }
}

static void
update_stock_icon_all()
{
    GList* list;

    for (list = pidgin_conv_windows_get_list();
         list;
         list = list->next)
    {
        PidginWindow* window = list->data;
        PidginConversation* gtkconv = window ? pidgin_conv_window_get_active_gtkconv(window) : NULL;

        update_stock_icon(gtkconv);
    }
}

static void
handle_conversation_switched(PurpleConversation* conv)
{
    purple_debug_info(PLUGIN_ID, "handle_conversation_switched\n");

    if (!pidgin_conv_is_hidden(PIDGIN_CONVERSATION(conv)))
    {
        update_stock_icon(PIDGIN_CONVERSATION(conv));
    }
}

static void
handle_toggle_presence_icon(gpointer data)
{
    purple_debug_info(PLUGIN_ID, "handle_toggle_presence_icon\n");

    purple_prefs_set_bool(
        PREF_SHOW_PRESENCE,
        !purple_prefs_get_bool(PREF_SHOW_PRESENCE));

    update_stock_icon_all();
}

static void
handle_conversation_extended_menu(PurpleConversation* conv, GList** list)
{
    purple_debug_info(PLUGIN_ID, "handle_conversation_extended_menu\n");

    if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
    {
        PurpleMenuAction* action = purple_menu_action_new(
            "Toggle Presence Icon",
            PURPLE_CALLBACK(handle_toggle_presence_icon),
            NULL /* data */,
            NULL /* children */);
        *list = g_list_append(*list, action);
    }
}

static gboolean
timeout_callback_update_stock_icon_all()
{
    purple_debug_info(PLUGIN_ID, "timeout_callback_update_stock_icon_all\n");

    update_stock_icon_all();
    return FALSE;
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

    purple_debug_info(PLUGIN_ID, "handle_jabber_receiving_presence %s %s\n", type, from);

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

    g_hash_table_replace(s_presence, (gpointer)g_strdup(from), GINT_TO_POINTER(state));

    if (purple_prefs_get_bool(PREF_SHOW_PRESENCE))
    {
        // invoke update_stock_icon_all() after handling of current signal is finished.
        purple_timeout_add_seconds(
            0 /* interval */,
            (GSourceFunc)timeout_callback_update_stock_icon_all,
            NULL /* data */);
    }

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

    s_original_stock = g_hash_table_new(
        g_str_hash,
        g_str_equal);

    s_muc_presence = plugin;

    purple_signal_connect(
        pidgin_conversations_get_handle(),
        "conversation-switched",
        plugin, PURPLE_CALLBACK(handle_conversation_switched), NULL);

    purple_signal_connect(
        purple_conversations_get_handle(),
        "conversation-extended-menu",
        plugin, PURPLE_CALLBACK(handle_conversation_extended_menu), NULL);

    if ((jabber = purple_find_prpl(PROTOCOL_JABBER)) != NULL)
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
    g_hash_table_destroy(s_original_stock);
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
    "1.1",

    "XMPP MUC Presence plugin",
    "Show status icon in chatroom",
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
    purple_prefs_add_none(PREF_PREFIX);
    purple_prefs_add_bool(PREF_SHOW_PRESENCE, TRUE);
}

PURPLE_INIT_PLUGIN(xmpp_muc_presence_plugin, init_plugin, info)
