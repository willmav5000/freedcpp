/*
 * Copyright © 2004-2008 Jens Oknelid, paskharen@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 *(at your option)any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * In addition, as a special exception, compiling, linking, and/or
 * using OpenSSL with this program is allowed.
 */

#include "settingsdialog.hh"

#include <dcpp/CryptoManager.h>
#include <dcpp/FavoriteManager.h>
#include <dcpp/NmdcHub.h>
#include <dcpp/ShareManager.h>
#include "settingsmanager.hh"
#include "sound.hh"
#include "wulformanager.hh"
#include "WulforUtil.hh"

using namespace std;
using namespace dcpp;

Settings::Settings(GtkWindow* parent):
	DialogEntry(Entry::SETTINGS_DIALOG, "settingsdialog.glade", parent)
{
	// Configure the dialogs.
	gtk_dialog_set_alternative_button_order(GTK_DIALOG(getWidget("dialog")), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);
	gtk_dialog_set_alternative_button_order(GTK_DIALOG(getWidget("favoriteNameDialog")), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);
	gtk_dialog_set_alternative_button_order(GTK_DIALOG(getWidget("publicHubsDialog")), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);
	gtk_dialog_set_alternative_button_order(GTK_DIALOG(getWidget("virtualNameDialog")), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);
	gtk_dialog_set_alternative_button_order(GTK_DIALOG(getWidget("dirChooserDialog")), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);
	gtk_dialog_set_alternative_button_order(GTK_DIALOG(getWidget("fileChooserDialog")), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);

	textStyleBuffer = gtk_text_buffer_new(NULL);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(getWidget("textViewPreviewStyles")), textStyleBuffer);

	// Initialize the tabs in the GtkNotebook.
	initPersonal_gui();
	initConnection_gui();
	initDownloads_gui();
	initSharing_gui();
	initAppearance_gui();
	initLog_gui();
	initAdvanced_gui();
}

Settings::~Settings()
{
	if (getResponseID() == GTK_RESPONSE_OK)
		saveSettings_client();

	gtk_widget_destroy(getWidget("favoriteNameDialog"));
	gtk_widget_destroy(getWidget("publicHubsDialog"));
	gtk_widget_destroy(getWidget("virtualNameDialog"));
	gtk_widget_destroy(getWidget("dirChooserDialog"));
	gtk_widget_destroy(getWidget("fileChooserDialog"));
	gtk_widget_destroy(getWidget("commandDialog"));
	gtk_widget_destroy(getWidget("fontSelectionDialog"));
	gtk_widget_destroy(getWidget("colorSelectionDialog"));
}

void Settings::saveSettings_client()
{
	SettingsManager *sm = SettingsManager::getInstance();
	WulforSettingsManager *wsm = WulforSettingsManager::getInstance();
	string path;

	{ // Personal
		sm->set(SettingsManager::NICK, gtk_entry_get_text(GTK_ENTRY(getWidget("nickEntry"))));
		sm->set(SettingsManager::EMAIL, gtk_entry_get_text(GTK_ENTRY(getWidget("emailEntry"))));
		sm->set(SettingsManager::DESCRIPTION, gtk_entry_get_text(GTK_ENTRY(getWidget("descriptionEntry"))));
		sm->set(SettingsManager::UPLOAD_SPEED, SettingsManager::connectionSpeeds[gtk_combo_box_get_active(connectionSpeedComboBox)]);

		gchar *encoding = gtk_combo_box_get_active_text(GTK_COMBO_BOX(getWidget("comboboxCharset")));
		WSET("default-charset", string(encoding));
		g_free(encoding);
	}

	{ // Connection
		// Incoming connection
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("activeRadioButton"))))
			sm->set(SettingsManager::INCOMING_CONNECTIONS, SettingsManager::INCOMING_DIRECT);
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("portForwardRadioButton"))))
			sm->set(SettingsManager::INCOMING_CONNECTIONS, SettingsManager::INCOMING_FIREWALL_NAT);
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("passiveRadioButton"))))
			sm->set(SettingsManager::INCOMING_CONNECTIONS, SettingsManager::INCOMING_FIREWALL_PASSIVE);

		sm->set(SettingsManager::EXTERNAL_IP, gtk_entry_get_text(GTK_ENTRY(getWidget("ipEntry"))));
		sm->set(SettingsManager::NO_IP_OVERRIDE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("forceIPCheckButton"))));

		int port = Util::toInt(gtk_entry_get_text(GTK_ENTRY(getWidget("tcpEntry"))));
		if (port > 0 && port <= 65535)
			sm->set(SettingsManager::TCP_PORT, port);
		port = Util::toInt(gtk_entry_get_text(GTK_ENTRY(getWidget("udpEntry"))));
		if (port > 0 && port <= 65535)
			sm->set(SettingsManager::UDP_PORT, port);
		port = Util::toInt(gtk_entry_get_text(GTK_ENTRY(getWidget("tlsEntry"))));
		if (port > 0 && port <= 65535)
			sm->set(SettingsManager::TLS_PORT, port);

		// Outgoing connection
		int type = SETTING(OUTGOING_CONNECTIONS);
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("outDirectRadioButton"))))
			sm->set(SettingsManager::OUTGOING_CONNECTIONS, SettingsManager::OUTGOING_DIRECT);
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("socksRadioButton"))))
			sm->set(SettingsManager::OUTGOING_CONNECTIONS, SettingsManager::OUTGOING_SOCKS5);

		if (SETTING(OUTGOING_CONNECTIONS) != type)
			Socket::socksUpdated();

		sm->set(SettingsManager::SOCKS_SERVER, gtk_entry_get_text(GTK_ENTRY(getWidget("socksIPEntry"))));
		sm->set(SettingsManager::SOCKS_USER, gtk_entry_get_text(GTK_ENTRY(getWidget("socksUserEntry"))));
		sm->set(SettingsManager::SOCKS_PASSWORD, gtk_entry_get_text(GTK_ENTRY(getWidget("socksPassEntry"))));
		sm->set(SettingsManager::SOCKS_RESOLVE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("socksCheckButton"))));

		port = Util::toInt(gtk_entry_get_text(GTK_ENTRY(getWidget("socksPortEntry"))));
		if (port > 0 && port <= 65535)
			sm->set(SettingsManager::SOCKS_PORT, port);
	}

	{ // Downloads
		path = gtk_entry_get_text(GTK_ENTRY(getWidget("finishedDownloadsEntry")));
		if (path[path.length() - 1] != PATH_SEPARATOR)
			path += PATH_SEPARATOR;
		sm->set(SettingsManager::DOWNLOAD_DIRECTORY, path);

		path = gtk_entry_get_text(GTK_ENTRY(getWidget("unfinishedDownloadsEntry")));
		if (!path.empty() && path[path.length() - 1] != PATH_SEPARATOR)
			path += PATH_SEPARATOR;
		sm->set(SettingsManager::TEMP_DOWNLOAD_DIRECTORY, path);
		sm->set(SettingsManager::DOWNLOAD_SLOTS, (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("maxDownloadsSpinButton"))));
		sm->set(SettingsManager::MAX_DOWNLOAD_SPEED, (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("newDownloadsSpinButton"))));
		sm->set(SettingsManager::HTTP_PROXY, gtk_entry_get_text(GTK_ENTRY(getWidget("proxyEntry"))));

		{ // Queue
			// Auto-priority
			sm->set(SettingsManager::PRIO_HIGHEST_SIZE, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("priorityHighestSpinButton")))));
			sm->set(SettingsManager::PRIO_HIGH_SIZE, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("priorityHighSpinButton")))));
			sm->set(SettingsManager::PRIO_NORMAL_SIZE, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("priorityNormalSpinButton")))));
			sm->set(SettingsManager::PRIO_LOW_SIZE, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("priorityLowSpinButton")))));

			// Auto-drop
			sm->set(SettingsManager::AUTODROP_SPEED, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("dropSpeedSpinButton")))));
			sm->set(SettingsManager::AUTODROP_ELAPSED, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("dropElapsedSpinButton")))));
			sm->set(SettingsManager::AUTODROP_MINSOURCES, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("dropMinSourcesSpinButton")))));
			sm->set(SettingsManager::AUTODROP_INTERVAL, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("dropCheckSpinButton")))));
			sm->set(SettingsManager::AUTODROP_INACTIVITY, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("dropInactiveSpinButton")))));
			sm->set(SettingsManager::AUTODROP_FILESIZE, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("dropSizeSpinButton")))));

			// Other queue options
			saveOptionsView_gui(queueView, sm);
		}
	}

	{ // Sharing
		sm->set(SettingsManager::FOLLOW_LINKS, (int)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("followLinksCheckButton"))));
		sm->set(SettingsManager::MIN_UPLOAD_SPEED, (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("sharedExtraSlotSpinButton"))));
		sm->set(SettingsManager::SLOTS, (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("sharedUploadSlotsSpinButton"))));
	}

	{ // Appearance
		saveOptionsView_gui(appearanceView, sm);

		WSET("tab-position", gtk_combo_box_get_active(GTK_COMBO_BOX(getWidget("tabPositionComboBox"))));
		WSET("toolbar-style", gtk_combo_box_get_active(GTK_COMBO_BOX(getWidget("toolbarStyleComboBox"))));

		sm->set(SettingsManager::DEFAULT_AWAY_MESSAGE, string(gtk_entry_get_text(GTK_ENTRY(getWidget("awayMessageEntry")))));
		sm->set(SettingsManager::TIME_STAMPS_FORMAT, string(gtk_entry_get_text(GTK_ENTRY(getWidget("timestampEntry")))));

		{ // Tabs
			saveOptionsView_gui(tabView, sm);
		}

		{ // Sounds
			GtkTreeIter iter;
			GtkTreeModel *m = GTK_TREE_MODEL(soundStore);
			gboolean valid = gtk_tree_model_get_iter_first(m, &iter);
			string sample, target;

			while (valid)
			{
				sample = soundView.getString(&iter, "Sample");
				target = soundView.getString(&iter, _("File"));
				WSET(sample, target);
				valid = gtk_tree_model_iter_next(m, &iter);
			}

			WSET("sound-pm",gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("soundPMReceivedCheckButton"))));
			WSET("sound-pm-open", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("soundPMWindowCheckButton"))));
		}

		{ // Colors & Fonts

			GtkTreeIter iter;
			GtkTreeModel *m = GTK_TREE_MODEL(textStyleStore);
			gboolean valid = gtk_tree_model_get_iter_first(m, &iter);

			while (valid)
			{
				wsm->set(textStyleView.getString(&iter, "keyForeColor"), textStyleView.getString(&iter, "ForeColor"));
				wsm->set(textStyleView.getString(&iter, "keyBackColor"), textStyleView.getString(&iter, "BackColor"));
				wsm->set(textStyleView.getString(&iter, "keyBolt"), textStyleView.getValue<int>(&iter, "Bolt"));
				wsm->set(textStyleView.getString(&iter, "keyItalic"), textStyleView.getValue<int>(&iter, "Italic"));

				valid = gtk_tree_model_iter_next(m, &iter);
			}

			WSET("text-bold-autors", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("checkBoldAuthors"))));
		}

		{ // Window
			// Auto-open on startup
			saveOptionsView_gui(windowView1, sm);

			// Window options
			saveOptionsView_gui(windowView2, sm);

			// Confirm dialog options
			saveOptionsView_gui(windowView3, sm);
		}
	}

	{ // Logs
		path = gtk_entry_get_text(GTK_ENTRY(getWidget("logDirectoryEntry")));
		if (!path.empty() && path[path.length() - 1] != PATH_SEPARATOR)
			path += PATH_SEPARATOR;
		sm->set(SettingsManager::LOG_DIRECTORY, path);
		sm->set(SettingsManager::LOG_MAIN_CHAT, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("logMainCheckButton"))));
		sm->set(SettingsManager::LOG_FORMAT_MAIN_CHAT, string(gtk_entry_get_text(GTK_ENTRY(getWidget("logMainEntry")))));
		sm->set(SettingsManager::LOG_PRIVATE_CHAT, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("logPrivateCheckButton"))));
		sm->set(SettingsManager::LOG_FORMAT_PRIVATE_CHAT, string(gtk_entry_get_text(GTK_ENTRY(getWidget("logPrivateEntry")))));
		sm->set(SettingsManager::LOG_DOWNLOADS, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("logDownloadsCheckButton"))));
		sm->set(SettingsManager::LOG_FORMAT_POST_DOWNLOAD, string(gtk_entry_get_text(GTK_ENTRY(getWidget("logDownloadsEntry")))));
		sm->set(SettingsManager::LOG_UPLOADS, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("logUploadsCheckButton"))));
		sm->set(SettingsManager::LOG_FORMAT_POST_UPLOAD, string(gtk_entry_get_text(GTK_ENTRY(getWidget("logUploadsEntry")))));
		sm->set(SettingsManager::LOG_SYSTEM, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("logSystemCheckButton"))));
		sm->set(SettingsManager::LOG_STATUS_MESSAGES, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("logStatusCheckButton"))));
		sm->set(SettingsManager::LOG_FILELIST_TRANSFERS, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("logFilelistTransfersCheckButton"))));
	}

	{ // Advanced
		saveOptionsView_gui(advancedView, sm);

		// Expert
		sm->set(SettingsManager::MAX_HASH_SPEED, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("hashSpeedSpinButton")))));
		sm->set(SettingsManager::BUFFER_SIZE, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("writeBufferSpinButton")))));
		sm->set(SettingsManager::SHOW_LAST_LINES_LOG, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("pmHistorySpinButton")))));
		sm->set(SettingsManager::SET_MINISLOT_SIZE, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("slotSizeSpinButton")))));
		sm->set(SettingsManager::MAX_FILELIST_SIZE, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("maxListSizeSpinButton")))));
		sm->set(SettingsManager::PRIVATE_ID, string(gtk_entry_get_text(GTK_ENTRY(getWidget("CIDEntry")))));
		sm->set(SettingsManager::AUTO_REFRESH_TIME, Util::toString (gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("autoRefreshSpinButton")))));
		sm->set(SettingsManager::SEARCH_HISTORY, Util::toString (gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("searchHistorySpinButton")))));
		sm->set(SettingsManager::BIND_ADDRESS, string(gtk_entry_get_text(GTK_ENTRY(getWidget("bindAddressEntry")))));
		sm->set(SettingsManager::SOCKET_IN_BUFFER, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("socketReadSpinButton")))));
		sm->set(SettingsManager::SOCKET_OUT_BUFFER, Util::toString(gtk_spin_button_get_value(GTK_SPIN_BUTTON(getWidget("socketWriteSpinButton")))));
		// Security Certificates
		path = gtk_entry_get_text(GTK_ENTRY(getWidget("trustedCertificatesPathEntry")));
		if (!path.empty() && path[path.length() - 1] != PATH_SEPARATOR)
			path += PATH_SEPARATOR;
		sm->set(SettingsManager::TLS_PRIVATE_KEY_FILE, string(gtk_entry_get_text(GTK_ENTRY(getWidget("privateKeyEntry")))));
		sm->set(SettingsManager::TLS_CERTIFICATE_FILE, string(gtk_entry_get_text(GTK_ENTRY(getWidget("certificateFileEntry")))));
		sm->set(SettingsManager::TLS_TRUSTED_CERTIFICATES_PATH, path);

		saveOptionsView_gui(certificatesView, sm);
	}

	sm->save();
}

/* Adds a core option */

void Settings::addOption_gui(GtkListStore *store, const string &name, SettingsManager::IntSetting setting)
{
	GtkTreeIter iter;
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
		0, SettingsManager::getInstance()->get(setting),
		1, name.c_str(),
		2, setting,
		3, "",
		-1);
}

/* Adds a custom UI specific option */

void Settings::addOption_gui(GtkListStore *store, const string &name, const string &setting)
{
	GtkTreeIter iter;
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
		0, WGETI(setting),
		1, name.c_str(),
		2, -2,
		3, setting.c_str(),
		-1);
}

/* Creates a generic sounds options GtkTreeView */

void Settings::addOption_gui(GtkListStore *store, WulforSettingsManager *wsm, const string &name, const string &key1)
{
	GtkTreeIter iter;
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
		0, name.c_str(),
		1, wsm->getString(key1).c_str(),
		2, key1.c_str(),
		-1);
}

/* Creates a generic colors and fonts options GtkTreeView */

void Settings::addOption_gui(GtkListStore *store, WulforSettingsManager *wsm, const string &name,
	const string &key1, const string &key2, const string &key3, const string &key4)
{
 	GtkTreeIter iter;
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
		0, name.c_str(),
		1, wsm->getString(key1).c_str(),
		2, wsm->getString(key2).c_str(),
		3, wsm->getInt(key3),
		4, wsm->getInt(key4),
		5, key1.c_str(),
		6, key2.c_str(),
		7, key3.c_str(),
		8, key4.c_str(),
		-1);
}

/* Creates a generic checkbox-based options GtkTreeView */

void Settings::createOptionsView_gui(TreeView &treeView, GtkListStore *&store, const string &widgetName)
{
	// Create the view
	treeView.setView(GTK_TREE_VIEW(getWidget(widgetName.c_str())));
	treeView.insertColumn("Use", G_TYPE_BOOLEAN, TreeView::BOOL, -1);
	treeView.insertColumn("Name", G_TYPE_STRING, TreeView::STRING, -1);
	treeView.insertHiddenColumn("Core Setting", G_TYPE_INT);
	treeView.insertHiddenColumn("UI Setting", G_TYPE_STRING);
	treeView.finalize();

	// Create the store
	store = gtk_list_store_newv(treeView.getColCount(), treeView.getGTypes());
	gtk_tree_view_set_model(treeView.get(), GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), treeView.col("Name"), GTK_SORT_ASCENDING);

	// Connect the signal handlers
	GList *list = gtk_tree_view_column_get_cell_renderers(gtk_tree_view_get_column(treeView.get(), treeView.col("Use")));
	GObject *renderer = (GObject *)g_list_nth_data(list, 0);
	g_signal_connect(renderer, "toggled", G_CALLBACK(onOptionsViewToggled_gui), (gpointer)store);
	g_list_free(list);
}

/* Saves the core or UI values stored in the options GtkTreeView */

void Settings::saveOptionsView_gui(TreeView &treeView, SettingsManager *sm)
{
	GtkTreeIter iter;
	GtkTreeModel *m = gtk_tree_view_get_model(treeView.get());
	gboolean valid = gtk_tree_model_get_iter_first(m, &iter);

	while (valid)
	{
		gboolean toggled = treeView.getValue<gboolean>(&iter, "Use");
		gint coreSetting = treeView.getValue<gint>(&iter, "Core Setting");

		// If core setting has been set to a valid value
		if (coreSetting >= 0)
		{
			sm->set((SettingsManager::IntSetting)coreSetting, toggled);
		}
		else
		{
			string uiSetting = treeView.getString(&iter, "UI Setting");
			WSET(uiSetting, toggled);
		}
		valid = gtk_tree_model_iter_next(m, &iter);
	}
}

void Settings::initPersonal_gui()
{
	gtk_entry_set_text(GTK_ENTRY(getWidget("nickEntry")), SETTING(NICK).c_str());
	gtk_entry_set_text(GTK_ENTRY(getWidget("emailEntry")), SETTING(EMAIL).c_str());
	gtk_entry_set_text(GTK_ENTRY(getWidget("descriptionEntry")), SETTING(DESCRIPTION).c_str());
	connectionSpeedComboBox = GTK_COMBO_BOX(gtk_combo_box_new_text());
	gtk_box_pack_start(GTK_BOX(getWidget("connectionBox")), GTK_WIDGET(connectionSpeedComboBox), FALSE, TRUE, 0);
	gtk_widget_show_all(GTK_WIDGET(connectionSpeedComboBox));

	for (StringIter i = SettingsManager::connectionSpeeds.begin(); i != SettingsManager::connectionSpeeds.end(); ++i)
	{
		gtk_combo_box_append_text(connectionSpeedComboBox, (*i).c_str());
		if (SETTING(UPLOAD_SPEED) == *i)
			gtk_combo_box_set_active(connectionSpeedComboBox, i - SettingsManager::connectionSpeeds.begin());
	}

	// Fill charset drop-down list
	vector<string> &charsets = WulforUtil::getCharsets();
	for (vector<string>::const_iterator it = charsets.begin(); it != charsets.end(); ++it)
		gtk_combo_box_append_text(GTK_COMBO_BOX(getWidget("comboboxCharset")), it->c_str());

	gtk_entry_set_text(GTK_ENTRY(getWidget("comboboxentryCharset")), WGETS("default-charset").c_str());
}

void Settings::initConnection_gui()
{
	// Incoming
	g_signal_connect(getWidget("activeRadioButton"), "toggled", G_CALLBACK(onInDirect_gui), (gpointer)this);
	///@todo Uncomment when implemented
	//g_signal_connect(getWidget("upnpRadioButton"), "toggled", G_CALLBACK(onInFW_UPnP_gui), (gpointer)this);
	g_signal_connect(getWidget("portForwardRadioButton"), "toggled", G_CALLBACK(onInFW_NAT_gui), (gpointer)this);
	g_signal_connect(getWidget("passiveRadioButton"), "toggled", G_CALLBACK(onInPassive_gui), (gpointer)this);
	gtk_entry_set_text(GTK_ENTRY(getWidget("ipEntry")), SETTING(EXTERNAL_IP).c_str());

	// Fill IP address combo box
	vector<string> addresses = WulforUtil::getLocalIPs();
	for (vector<string>::const_iterator it = addresses.begin(); it != addresses.end(); ++it)
		gtk_combo_box_append_text(GTK_COMBO_BOX(getWidget("ipComboboxEntry")), it->c_str());

	gtk_entry_set_text(GTK_ENTRY(getWidget("tcpEntry")), Util::toString(SETTING(TCP_PORT)).c_str());
	gtk_entry_set_text(GTK_ENTRY(getWidget("udpEntry")), Util::toString(SETTING(UDP_PORT)).c_str());
	gtk_entry_set_text(GTK_ENTRY(getWidget("tlsEntry")), Util::toString(SETTING(TLS_PORT)).c_str());
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("forceIPCheckButton")), SETTING(NO_IP_OVERRIDE));

	switch (SETTING(INCOMING_CONNECTIONS))
	{
		case SettingsManager::INCOMING_DIRECT:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("activeRadioButton")), TRUE);
			break;
		case SettingsManager::INCOMING_FIREWALL_NAT:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("portForwardRadioButton")), TRUE);
			break;
		case SettingsManager::INCOMING_FIREWALL_UPNP:
			///@todo: implement
			break;
		case SettingsManager::INCOMING_FIREWALL_PASSIVE:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("passiveRadioButton")), TRUE);
			break;
	}

	// Outgoing
	g_signal_connect(getWidget("outDirectRadioButton"), "toggled", G_CALLBACK(onOutDirect_gui), (gpointer)this);
	g_signal_connect(getWidget("socksRadioButton"), "toggled", G_CALLBACK(onSocks5_gui), (gpointer)this);
	gtk_entry_set_text(GTK_ENTRY(getWidget("socksIPEntry")), SETTING(SOCKS_SERVER).c_str());
	gtk_entry_set_text(GTK_ENTRY(getWidget("socksUserEntry")), SETTING(SOCKS_USER).c_str());
	gtk_entry_set_text(GTK_ENTRY(getWidget("socksPortEntry")), Util::toString(SETTING(SOCKS_PORT)).c_str());
	gtk_entry_set_text(GTK_ENTRY(getWidget("socksPassEntry")), SETTING(SOCKS_PASSWORD).c_str());
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("socksCheckButton")), SETTING(SOCKS_RESOLVE));

	switch (SETTING(OUTGOING_CONNECTIONS))
	{
		case SettingsManager::OUTGOING_DIRECT:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("outDirectRadioButton")), TRUE);
			onOutDirect_gui(NULL, (gpointer)this);
			break;
		case SettingsManager::OUTGOING_SOCKS5:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("socksRadioButton")), TRUE);
			break;
	}
}

void Settings::initDownloads_gui()
{
	GtkTreeIter iter;

	{ // Downloads
		g_signal_connect(getWidget("finishedDownloadsButton"), "clicked", G_CALLBACK(onBrowseFinished_gui), (gpointer)this);
		g_signal_connect(getWidget("unfinishedDownloadsButton"), "clicked", G_CALLBACK(onBrowseUnfinished_gui), (gpointer)this);
		g_signal_connect(getWidget("publicHubsButton"), "clicked", G_CALLBACK(onPublicHubs_gui), (gpointer)this);
		g_signal_connect(getWidget("publicHubsDialogAddButton"), "clicked", G_CALLBACK(onPublicAdd_gui), (gpointer)this);
		g_signal_connect(getWidget("publicHubsDialogUpButton"), "clicked", G_CALLBACK(onPublicMoveUp_gui), (gpointer)this);
		g_signal_connect(getWidget("publicHubsDialogDownButton"), "clicked", G_CALLBACK(onPublicMoveDown_gui), (gpointer)this);
		g_signal_connect(getWidget("publicHubsDialogRemoveButton"), "clicked", G_CALLBACK(onPublicRemove_gui), (gpointer)this);

		gtk_entry_set_text(GTK_ENTRY(getWidget("finishedDownloadsEntry")), SETTING(DOWNLOAD_DIRECTORY).c_str());
		gtk_entry_set_text(GTK_ENTRY(getWidget("unfinishedDownloadsEntry")), SETTING(TEMP_DOWNLOAD_DIRECTORY).c_str());
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("maxDownloadsSpinButton")), (double)SETTING(DOWNLOAD_SLOTS));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("newDownloadsSpinButton")), (double)SETTING(MAX_DOWNLOAD_SPEED));
		gtk_entry_set_text(GTK_ENTRY(getWidget("proxyEntry")), SETTING(HTTP_PROXY).c_str());

		publicListView.setView(GTK_TREE_VIEW(getWidget("publicHubsDialogTreeView")));
		publicListView.insertColumn("List", G_TYPE_STRING, TreeView::EDIT_STRING, -1);
		publicListView.finalize();
		publicListStore = gtk_list_store_newv(publicListView.getColCount(), publicListView.getGTypes());
		gtk_tree_view_set_model(publicListView.get(), GTK_TREE_MODEL(publicListStore));
		g_object_unref(publicListStore);
		gtk_tree_view_set_headers_visible(publicListView.get(), FALSE);
		GtkTreeViewColumn *col = gtk_tree_view_get_column(publicListView.get(), 0);
		GList *list = gtk_tree_view_column_get_cell_renderers(col);
		GObject *editRenderer = G_OBJECT(g_list_nth_data(list, 0));
		g_list_free(list);
		g_signal_connect(editRenderer, "edited", G_CALLBACK(onPublicEdit_gui), (gpointer)this);
	}

	{ // Download to
		g_signal_connect(getWidget("favoriteAddButton"), "clicked", G_CALLBACK(onAddFavorite_gui), (gpointer)this);
		g_signal_connect(getWidget("favoriteRemoveButton"), "clicked", G_CALLBACK(onRemoveFavorite_gui), (gpointer)this);
		downloadToView.setView(GTK_TREE_VIEW(getWidget("favoriteTreeView")));
		downloadToView.insertColumn("Favorite Name", G_TYPE_STRING, TreeView::STRING, -1);
		downloadToView.insertColumn("Directory", G_TYPE_STRING, TreeView::STRING, -1);
		downloadToView.finalize();
		downloadToStore = gtk_list_store_newv(downloadToView.getColCount(), downloadToView.getGTypes());
		gtk_tree_view_set_model(downloadToView.get(), GTK_TREE_MODEL(downloadToStore));
		g_object_unref(downloadToStore);
		g_signal_connect(downloadToView.get(), "button-release-event", G_CALLBACK(onFavoriteButtonReleased_gui), (gpointer)this);
		gtk_widget_set_sensitive(getWidget("favoriteRemoveButton"), FALSE);

		StringPairList directories = FavoriteManager::getInstance()->getFavoriteDirs();
		for (StringPairIter j = directories.begin(); j != directories.end(); ++j)
		{
			gtk_list_store_append(downloadToStore, &iter);
			gtk_list_store_set(downloadToStore, &iter,
				downloadToView.col("Favorite Name"), j->second.c_str(),
				downloadToView.col("Directory"), j->first.c_str(),
				-1);
		}
	}

	{ // Preview
		previewAppView.setView(GTK_TREE_VIEW(getWidget("previewTreeView")));
		previewAppView.insertColumn(_("Name"), G_TYPE_STRING, TreeView::STRING, -1);
		previewAppView.insertColumn(_("Application"), G_TYPE_STRING, TreeView::STRING, -1);
		previewAppView.insertColumn(_("Extensions"), G_TYPE_STRING, TreeView::STRING, -1);
		previewAppView.finalize();

		g_signal_connect(getWidget("previewAddButton"), "clicked", G_CALLBACK(onPreviewAdd_gui), (gpointer)this);
		g_signal_connect(getWidget("previewRemoveButton"), "clicked", G_CALLBACK(onPreviewRemove_gui), (gpointer)this);
		g_signal_connect(getWidget("previewApplyButton"), "clicked", G_CALLBACK(onPreviewApply_gui), (gpointer)this);
		g_signal_connect(getWidget("previewTreeView"), "key-release-event", G_CALLBACK(onPreviewKeyReleased_gui), (gpointer)this);
		g_signal_connect(previewAppView.get(), "button-release-event", G_CALLBACK(onPreviewButtonReleased_gui), (gpointer)this);

		previewAppToStore = gtk_list_store_newv(previewAppView.getColCount(), previewAppView.getGTypes());
		gtk_tree_view_set_model(previewAppView.get(), GTK_TREE_MODEL(previewAppToStore));
		g_object_unref(previewAppToStore);

		gtk_widget_set_sensitive(getWidget("previewAddButton"), TRUE);
		gtk_widget_set_sensitive(getWidget("previewRemoveButton"), TRUE);
		gtk_widget_set_sensitive(getWidget("previewApplyButton"), TRUE);

		WulforSettingsManager *wsm = WulforSettingsManager::getInstance();
		const PreviewApp::List &Apps = wsm->getPreviewApps();

		// add default applications players
		if (Apps.empty())
		{
			wsm->addPreviewApp(_("Xine player"), "xine --no-logo --session volume=50", "avi; mov; vob; mpg; mp3");
			wsm->addPreviewApp(_("Kaffeine player"), "kaffeine -p", "avi; mov; mpg; vob; mp3");
			wsm->addPreviewApp(_("Mplayer player"), "mplayer", "avi; mov; vob; mp3");
			wsm->addPreviewApp(_("Amarok player"), "amarok", "mp3");
		}

		for (PreviewApp::Iter item = Apps.begin(); item != Apps.end(); ++item)
		{
			gtk_list_store_append(previewAppToStore, &iter);
			gtk_list_store_set(previewAppToStore, &iter,
				previewAppView.col(_("Name")), ((*item)->name).c_str(),
				previewAppView.col(_("Application")), ((*item)->app).c_str(),
				previewAppView.col(_("Extensions")), ((*item)->ext).c_str(),
				-1);
		}
	}

	{ // Queue
		// Auto-priority
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("priorityHighestSpinButton")), (double)SETTING(PRIO_HIGHEST_SIZE));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("priorityHighSpinButton")), (double)SETTING(PRIO_HIGH_SIZE));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("priorityNormalSpinButton")), (double)SETTING(PRIO_NORMAL_SIZE));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("priorityLowSpinButton")), (double)SETTING(PRIO_LOW_SIZE));

		// Auto-drop
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("dropSpeedSpinButton")), (double)SETTING(AUTODROP_SPEED));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("dropElapsedSpinButton")), (double)SETTING(AUTODROP_ELAPSED));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("dropMinSourcesSpinButton")), (double)SETTING(AUTODROP_MINSOURCES));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("dropCheckSpinButton")), (double)SETTING(AUTODROP_INTERVAL));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("dropInactiveSpinButton")), (double)SETTING(AUTODROP_INACTIVITY));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("dropSizeSpinButton")), (double)SETTING(AUTODROP_FILESIZE));

		// Other queue options
		createOptionsView_gui(queueView, queueStore, "queueOtherTreeView");

		addOption_gui(queueStore, _("Set lowest priority for newly added files larger than low priority size"), SettingsManager::PRIO_LOWEST);
		addOption_gui(queueStore, _("Auto-drop slow sources for all queue items (except filelists)"), SettingsManager::AUTODROP_ALL);
		addOption_gui(queueStore, _("Remove slow filelists"), SettingsManager::AUTODROP_FILELISTS);
		addOption_gui(queueStore, _("Don't remove the source when auto-dropping, only disconnect"), SettingsManager::AUTODROP_DISCONNECT);
		addOption_gui(queueStore, _("Automatically search for alternative download locations"), SettingsManager::AUTO_SEARCH);
		addOption_gui(queueStore, _("Automatically match queue for auto search hits"), SettingsManager::AUTO_SEARCH_AUTO_MATCH);
		addOption_gui(queueStore, _("Skip zero-byte files"), SettingsManager::SKIP_ZERO_BYTE);
		addOption_gui(queueStore, _("Don't download files already in share"), SettingsManager::DONT_DL_ALREADY_SHARED);
		addOption_gui(queueStore, _("Don't download files already in the queue"), SettingsManager::DONT_DL_ALREADY_QUEUED);
	}
}

void Settings::initSharing_gui()
{
	g_signal_connect(getWidget("shareHiddenCheckButton"), "toggled", G_CALLBACK(onShareHiddenPressed_gui), (gpointer)this);
	g_signal_connect(getWidget("sharedAddButton"), "clicked", G_CALLBACK(onAddShare_gui), (gpointer)this);
	g_signal_connect(getWidget("sharedRemoveButton"), "clicked", G_CALLBACK(onRemoveShare_gui), (gpointer)this);

	shareView.setView(GTK_TREE_VIEW(getWidget("sharedTreeView")));
	shareView.insertColumn("Virtual Name", G_TYPE_STRING, TreeView::STRING, -1);
	shareView.insertColumn("Directory", G_TYPE_STRING, TreeView::STRING, -1);
	shareView.insertColumn("Size", G_TYPE_STRING, TreeView::STRING, -1);
	shareView.insertHiddenColumn("Real Size", G_TYPE_INT64);
	shareView.finalize();
	shareStore = gtk_list_store_newv(shareView.getColCount(), shareView.getGTypes());
	gtk_tree_view_set_model(shareView.get(), GTK_TREE_MODEL(shareStore));
	g_object_unref(shareStore);
	shareView.setSortColumn_gui("Size", "Real Size");
	g_signal_connect(shareView.get(), "button-release-event", G_CALLBACK(onShareButtonReleased_gui), (gpointer)this);
	gtk_widget_set_sensitive(getWidget("sharedRemoveButton"), FALSE);

	GtkTreeIter iter;
	StringPairList directories = ShareManager::getInstance()->getDirectories();
	for (StringPairList::iterator it = directories.begin(); it != directories.end(); ++it)
	{
		gtk_list_store_append(shareStore, &iter);
		gtk_list_store_set(shareStore, &iter,
			shareView.col("Virtual Name"), it->first.c_str(),
			shareView.col("Directory"), it->second.c_str(),
			shareView.col("Size"), Util::formatBytes(ShareManager::getInstance()->getShareSize(it->second)).c_str(),
			shareView.col("Real Size"), ShareManager::getInstance()->getShareSize(it->second),
			-1);
	}

	gtk_label_set_text(GTK_LABEL(getWidget("sharedSizeLabel")), string("Total size: " + Util::formatBytes(ShareManager::getInstance()->getShareSize())).c_str());
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("shareHiddenCheckButton")), BOOLSETTING(SHARE_HIDDEN));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("followLinksCheckButton")), BOOLSETTING(FOLLOW_LINKS));
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("sharedExtraSlotSpinButton")), (double)SETTING(MIN_UPLOAD_SPEED));
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("sharedUploadSlotsSpinButton")), (double)SETTING(SLOTS));
}

void Settings::initAppearance_gui()
{
	WulforSettingsManager *wsm = WulforSettingsManager::getInstance();

	{ // Appearance
		createOptionsView_gui(appearanceView, appearanceStore, "appearanceOptionsTreeView");

		addOption_gui(appearanceStore, _("Filter kick and NMDC debug messages"), SettingsManager::FILTER_MESSAGES);
		addOption_gui(appearanceStore, _("Show status icon"), SettingsManager::ALWAYS_TRAY);
		addOption_gui(appearanceStore, _("Show timestamps in chat by default"), SettingsManager::TIME_STAMPS);
		addOption_gui(appearanceStore, _("View status messages in main chat"), SettingsManager::STATUS_IN_CHAT);
		addOption_gui(appearanceStore, _("Show joins / parts in chat by default"), SettingsManager::SHOW_JOINS);
		addOption_gui(appearanceStore, _("Only show joins / parts for favorite users"), SettingsManager::FAV_SHOW_JOINS);
		addOption_gui(appearanceStore, _("Use OEM monospaced font for chat windows"), SettingsManager::USE_OEM_MONOFONT);
		addOption_gui(appearanceStore, _("Use magnet split"), "use-magnet-split");
		/// @todo: Uncomment when implemented
		//addOption_gui(appearanceStore, _("Minimize to tray"), SettingsManager::MINIMIZE_TRAY);
		//addOption_gui(appearanceStore, _("Use system icons"), SettingsManager::USE_SYSTEM_ICONS);

		gtk_combo_box_set_active(GTK_COMBO_BOX(getWidget("tabPositionComboBox")), WGETI("tab-position"));
		gtk_combo_box_set_active(GTK_COMBO_BOX(getWidget("toolbarStyleComboBox")), WGETI("toolbar-style"));

		gtk_entry_set_text(GTK_ENTRY(getWidget("awayMessageEntry")), SETTING(DEFAULT_AWAY_MESSAGE).c_str());
		gtk_entry_set_text(GTK_ENTRY(getWidget("timestampEntry")), SETTING(TIME_STAMPS_FORMAT).c_str());
	}

	{ // Tabs
		createOptionsView_gui(tabView, tabStore, "tabBoldingTreeView");

		addOption_gui(tabStore, _("Finished Downloads"), SettingsManager::BOLD_FINISHED_DOWNLOADS);
		addOption_gui(tabStore, _("Finished Uploads"), SettingsManager::BOLD_FINISHED_UPLOADS);
		addOption_gui(tabStore, _("Download Queue"), SettingsManager::BOLD_QUEUE);
		addOption_gui(tabStore, _("Hub (also sets urgency hint)"), SettingsManager::BOLD_HUB);
		addOption_gui(tabStore, _("Private Message (also sets urgency hint)"), SettingsManager::BOLD_PM);
		addOption_gui(tabStore, _("Search"), SettingsManager::BOLD_SEARCH);
	}

	{ // Sounds
		g_signal_connect(getWidget("soundPlayButton"), "clicked", G_CALLBACK(onSoundPlayButton_gui), (gpointer)this);
		g_signal_connect(getWidget("soundNoneButton"), "clicked", G_CALLBACK(onSoundNoneButton_gui), (gpointer)this);
		g_signal_connect(getWidget("soundFileBrowseButton"), "clicked", G_CALLBACK(onSoundFileBrowseClicked_gui), (gpointer)this);

		soundView.setView(GTK_TREE_VIEW(getWidget("soundsTreeView")));
		soundView.insertColumn(_("Sounds"), G_TYPE_STRING, TreeView::STRING, -1);
		soundView.insertColumn(_("File"), G_TYPE_STRING, TreeView::STRING, -1);
		soundView.insertHiddenColumn("Sample", G_TYPE_STRING);
		soundView.finalize();

		soundStore = gtk_list_store_newv(soundView.getColCount(), soundView.getGTypes());
		gtk_tree_view_set_model(soundView.get(), GTK_TREE_MODEL(soundStore));
		g_object_unref(soundStore);

		addOption_gui(soundStore, wsm, _("Download begins"), "sound-download-begins");
		addOption_gui(soundStore, wsm, _("Download finished"), "sound-download-finished");
		addOption_gui(soundStore, wsm, _("Upload finished"), "sound-upload-finished");
		addOption_gui(soundStore, wsm, _("Private message"), "sound-private-message");
		addOption_gui(soundStore, wsm, _("Hub connected"), "sound-hub-connect");
		addOption_gui(soundStore, wsm, _("Hub disconnected"), "sound-hub-disconnect");

		gtk_widget_set_sensitive(getWidget("soundPlayButton"), TRUE);
		gtk_widget_set_sensitive(getWidget("soundNoneButton"), TRUE);
		gtk_widget_set_sensitive(getWidget("soundFileBrowseButton"), TRUE);

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("soundPMReceivedCheckButton")), WGETB("sound-pm"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("soundPMWindowCheckButton")), WGETB("sound-pm-open"));
	}

	{ // Colors & Fonts
		g_signal_connect(getWidget("textColorForeButton"), "clicked", G_CALLBACK(onTextColorForeClicked_gui), (gpointer)this);
		g_signal_connect(getWidget("textColorBackButton"), "clicked", G_CALLBACK(onTextColorBackClicked_gui), (gpointer)this);
		g_signal_connect(getWidget("textColorBWButton"), "clicked", G_CALLBACK(onTextColorBWClicked_gui), (gpointer)this);
		g_signal_connect(getWidget("textStyleButton"), "clicked", G_CALLBACK(onTextStyleClicked_gui), (gpointer)this);
		g_signal_connect(getWidget("textStylesDefaultButton"), "clicked", G_CALLBACK(onTextStyleDefaultClicked_gui), (gpointer)this);

		textStyleView.setView(GTK_TREE_VIEW(getWidget("treeViewAvailableStyles")));
		textStyleView.insertColumn("Style", G_TYPE_STRING, TreeView::STRING, -1);
		textStyleView.insertHiddenColumn("ForeColor", G_TYPE_STRING);
		textStyleView.insertHiddenColumn("BackColor", G_TYPE_STRING);
		textStyleView.insertHiddenColumn("Bolt", G_TYPE_INT);
		textStyleView.insertHiddenColumn("Italic", G_TYPE_INT);
		textStyleView.insertHiddenColumn("keyForeColor", G_TYPE_STRING);
		textStyleView.insertHiddenColumn("keyBackColor", G_TYPE_STRING);
		textStyleView.insertHiddenColumn("keyBolt", G_TYPE_STRING);
		textStyleView.insertHiddenColumn("keyItalic", G_TYPE_STRING);
		textStyleView.finalize();

		textStyleStore = gtk_list_store_newv(textStyleView.getColCount(), textStyleView.getGTypes());
		gtk_tree_view_set_model(textStyleView.get(), GTK_TREE_MODEL(textStyleStore));
		g_object_unref(textStyleStore);

		// Available styles
		addOption_gui(textStyleStore, wsm, _("General text"),
			"text-general-fore-color", "text-general-back-color", "text-general-bold", "text-general-italic");

		addOption_gui(textStyleStore, wsm, _("My nick"),
			"text-mynick-fore-color", "text-mynick-back-color", "text-mynick-bold", "text-mynick-italic");

		addOption_gui(textStyleStore, wsm, _("My own message"),
			"text-myown-fore-color", "text-myown-back-color", "text-myown-bold", "text-myown-italic");

		addOption_gui(textStyleStore, wsm, _("Private message"),
			"text-private-fore-color", "text-private-back-color", "text-private-bold", "text-private-italic");

		addOption_gui(textStyleStore, wsm, _("System message"),
			"text-system-fore-color", "text-system-back-color", "text-system-bold", "text-system-italic");

		addOption_gui(textStyleStore, wsm, _("Status message"),
			"text-status-fore-color", "text-status-back-color", "text-status-bold", "text-status-italic");

		addOption_gui(textStyleStore, wsm, _("Timestamp"),
			"text-timestamp-fore-color", "text-timestamp-back-color", "text-timestamp-bold", "text-timestamp-italic");

		addOption_gui(textStyleStore, wsm, _("URL"),
			"text-url-fore-color", "text-url-back-color", "text-url-bold", "text-url-italic");

		addOption_gui(textStyleStore, wsm, _("Favorite User"),
			"text-fav-fore-color", "text-fav-back-color", "text-fav-bold", "text-fav-italic");

		addOption_gui(textStyleStore, wsm, _("Operator"),
			"text-op-fore-color", "text-op-back-color", "text-op-bold", "text-op-italic");

		// Preview style
		GtkTreeIter treeIter;
		GtkTreeModel *m = GTK_TREE_MODEL(textStyleStore);
		gboolean valid = gtk_tree_model_get_iter_first(m, &treeIter);

		GtkTextIter textIter, textStartIter, textEndIter;
		string line, style;

		string timestamp = "[" + Util::getShortTimeString() + "] ";
		const gint ts_strlen = g_utf8_strlen(timestamp.c_str(), -1);
		string fore = wsm->getString("text-timestamp-fore-color");
		string back = wsm->getString("text-timestamp-back-color");
		int bold = wsm->getInt("text-timestamp-bold");
		int italic = wsm->getInt("text-timestamp-italic");

		GtkTextTag *tag = NULL;
		GtkTextTag *tagTimeStamp = gtk_text_buffer_create_tag(textStyleBuffer, _("Timestamp"),
			"background", back.c_str(),
			"foreground", fore.c_str(),
			"weight", (PangoWeight)bold,
			"style", (PangoStyle)italic,
			NULL);

		gint row_count = 0;

		while (valid)
		{
			style = textStyleView.getString(&treeIter, "Style");
			fore = wsm->getString(textStyleView.getString(&treeIter, "keyForeColor"));
			back = wsm->getString(textStyleView.getString(&treeIter, "keyBackColor"));
			bold = wsm->getInt(textStyleView.getString(&treeIter, "keyBolt"));
			italic = wsm->getInt(textStyleView.getString(&treeIter, "keyItalic"));
			const gint st_strlen = g_utf8_strlen(style.c_str(), -1);

			line = timestamp + style + "\n";

			gtk_text_buffer_get_end_iter(textStyleBuffer, &textIter);
			gtk_text_buffer_insert(textStyleBuffer, &textIter, line.c_str(), line.size());

			textStartIter = textEndIter = textIter;

			// apply text style
			gtk_text_iter_backward_chars(&textStartIter, st_strlen + 1);

			if (row_count != 6)
				tag = gtk_text_buffer_create_tag(textStyleBuffer, style.c_str(),
					"background", back.c_str(),
					"foreground", fore.c_str(),
					"weight", (PangoWeight)bold,
					"style", (PangoStyle)italic,
					NULL);
			else
				tag = tagTimeStamp;

			gtk_text_buffer_apply_tag(textStyleBuffer, tag, &textStartIter, &textEndIter);

			// apply timestamp style
			gtk_text_iter_backward_chars(&textStartIter, ts_strlen);
			gtk_text_iter_backward_chars(&textEndIter, st_strlen + 2);
			gtk_text_buffer_apply_tag(textStyleBuffer, tagTimeStamp, &textStartIter, &textEndIter);

			row_count++;

			valid = gtk_tree_model_iter_next(m, &treeIter);
		}

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("checkBoldAuthors")), WGETB("text-bold-autors"));
	}

	{ // Window
		// Auto-open
		createOptionsView_gui(windowView1, windowStore1, "windowsAutoOpenTreeView");

		addOption_gui(windowStore1, _("Public Hubs"), SettingsManager::OPEN_PUBLIC);
		addOption_gui(windowStore1, _("Favorite Hubs"), SettingsManager::OPEN_FAVORITE_HUBS);
		addOption_gui(windowStore1, _("Download Queue"), SettingsManager::OPEN_QUEUE);
		addOption_gui(windowStore1, _("Finished Downloads"), SettingsManager::OPEN_FINISHED_DOWNLOADS);
		addOption_gui(windowStore1, _("Finished Uploads"), SettingsManager::OPEN_FINISHED_UPLOADS);
		/// @todo: Uncomment when implemented
		//addOption_gui(windowStore1, _("Favorite Users"), SettingsManager::OPEN_FAVORITE_USERS);

		// Window options
		createOptionsView_gui(windowView2, windowStore2, "windowsOptionsTreeView");

		addOption_gui(windowStore2, _("Open file list window in the background"), SettingsManager::POPUNDER_FILELIST);
		addOption_gui(windowStore2, _("Open new private messages from other users in the background"), SettingsManager::POPUNDER_PM);
		addOption_gui(windowStore2, _("Open new window when using /join"), SettingsManager::JOIN_OPEN_NEW_WINDOW);
		addOption_gui(windowStore2, _("Ignore private messages from the hub"), SettingsManager::IGNORE_HUB_PMS);
		addOption_gui(windowStore2, _("Ignore private messages from bots"), SettingsManager::IGNORE_BOT_PMS);

		// Confirmation dialog
		createOptionsView_gui(windowView3, windowStore3, "windowsConfirmTreeView");

		addOption_gui(windowStore3, _("Confirm application exit"), SettingsManager::CONFIRM_EXIT);
		addOption_gui(windowStore3, _("Confirm favorite hub removal"), SettingsManager::CONFIRM_HUB_REMOVAL);
		/// @todo: Uncomment when implemented
		//addOption_gui(windowStore3, _("Confirm item removal in download queue"), SettingsManager::CONFIRM_ITEM_REMOVAL);
	}
}

void Settings::initLog_gui()
{
	g_signal_connect(getWidget("logBrowseButton"), "clicked", G_CALLBACK(onLogBrowseClicked_gui), (gpointer)this);
	gtk_entry_set_text(GTK_ENTRY(getWidget("logDirectoryEntry")), SETTING(LOG_DIRECTORY).c_str());

	g_signal_connect(getWidget("logMainCheckButton"), "toggled", G_CALLBACK(onLogMainClicked_gui), (gpointer)this);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("logMainCheckButton")), BOOLSETTING(LOG_MAIN_CHAT));
	gtk_entry_set_text(GTK_ENTRY(getWidget("logMainEntry")), SETTING(LOG_FORMAT_MAIN_CHAT).c_str());
	gtk_widget_set_sensitive(getWidget("logMainLabel"), BOOLSETTING(LOG_MAIN_CHAT));
	gtk_widget_set_sensitive(getWidget("logMainEntry"), BOOLSETTING(LOG_MAIN_CHAT));

	g_signal_connect(getWidget("logPrivateCheckButton"), "toggled", G_CALLBACK(onLogPrivateClicked_gui), (gpointer)this);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("logPrivateCheckButton")), BOOLSETTING(LOG_PRIVATE_CHAT));
	gtk_entry_set_text(GTK_ENTRY(getWidget("logPrivateEntry")), SETTING(LOG_FORMAT_PRIVATE_CHAT).c_str());
	gtk_widget_set_sensitive(getWidget("logPrivateLabel"), BOOLSETTING(LOG_PRIVATE_CHAT));
	gtk_widget_set_sensitive(getWidget("logPrivateEntry"), BOOLSETTING(LOG_PRIVATE_CHAT));

	g_signal_connect(getWidget("logDownloadsCheckButton"), "toggled", G_CALLBACK(onLogDownloadClicked_gui), (gpointer)this);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("logDownloadsCheckButton")), BOOLSETTING(LOG_DOWNLOADS));
	gtk_entry_set_text(GTK_ENTRY(getWidget("logDownloadsEntry")), SETTING(LOG_FORMAT_POST_DOWNLOAD).c_str());
	gtk_widget_set_sensitive(getWidget("logDownloadsLabel"), BOOLSETTING(LOG_DOWNLOADS));
	gtk_widget_set_sensitive(getWidget("logDownloadsEntry"), BOOLSETTING(LOG_DOWNLOADS));

	g_signal_connect(getWidget("logUploadsCheckButton"), "toggled", G_CALLBACK(onLogUploadClicked_gui), (gpointer)this);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("logUploadsCheckButton")), BOOLSETTING(LOG_UPLOADS));
	gtk_entry_set_text(GTK_ENTRY(getWidget("logUploadsEntry")), SETTING(LOG_FORMAT_POST_UPLOAD).c_str());
	gtk_widget_set_sensitive(getWidget("logUploadsLabel"), BOOLSETTING(LOG_UPLOADS));
	gtk_widget_set_sensitive(getWidget("logUploadsEntry"), BOOLSETTING(LOG_UPLOADS));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("logSystemCheckButton")), BOOLSETTING(LOG_SYSTEM));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("logStatusCheckButton")), BOOLSETTING(LOG_STATUS_MESSAGES));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(getWidget("logFilelistTransfersCheckButton")), BOOLSETTING(LOG_FILELIST_TRANSFERS));
}

void Settings::initAdvanced_gui()
{
	{ // Advanced
		createOptionsView_gui(advancedView, advancedStore, "advancedTreeView");

		addOption_gui(advancedStore, _("Auto-away on minimize (and back on restore)"), SettingsManager::AUTO_AWAY);
		addOption_gui(advancedStore, _("Automatically follow redirects"), SettingsManager::AUTO_FOLLOW);
		addOption_gui(advancedStore, _("Clear search box after each search"), SettingsManager::CLEAR_SEARCH);
		addOption_gui(advancedStore, _("Keep duplicate files in your file list (duplicates never count towards your share size)"), SettingsManager::LIST_DUPES);
		addOption_gui(advancedStore, _("Don't delete file lists when exiting"), SettingsManager::KEEP_LISTS);
		addOption_gui(advancedStore, _("Automatically disconnect users who leave the hub"), SettingsManager::AUTO_KICK);
		addOption_gui(advancedStore, _("Enable segmented downloads"), SettingsManager::SEGMENTED_DL);
		addOption_gui(advancedStore, _("Enable automatic SFV checking"), SettingsManager::SFV_CHECK);
		addOption_gui(advancedStore, _("Enable safe and compressed transfers"), SettingsManager::COMPRESS_TRANSFERS);
		addOption_gui(advancedStore, _("Accept custom user commands from hub"), SettingsManager::HUB_USER_COMMANDS);
		addOption_gui(advancedStore, _("Send unknown /commands to the hub"), SettingsManager::SEND_UNKNOWN_COMMANDS);
		addOption_gui(advancedStore, _("Add finished files to share instantly (if shared)"), SettingsManager::ADD_FINISHED_INSTANTLY);
		addOption_gui(advancedStore, _("Don't send the away message to bots"), SettingsManager::NO_AWAYMSG_TO_BOTS);
		addOption_gui(advancedStore, _("Register with the OS to handle dchub:// and adc:// URL links"), SettingsManager::URL_HANDLER);
		addOption_gui(advancedStore, _("Register with the OS to handle magnet: URL links"), SettingsManager::MAGNET_REGISTER);
		/// @todo: Uncomment when implemented
		//addOption_gui(advancedStore, _("Use CTRL for line history"), SettingsManager::USE_CTRL_FOR_LINE_HISTORY);
	}

	{ // User Commands
		userCommandView.setView(GTK_TREE_VIEW(getWidget("userCommandTreeView")));
		userCommandView.insertColumn("Name", G_TYPE_STRING, TreeView::STRING, -1);
		userCommandView.insertColumn("Hub", G_TYPE_STRING, TreeView::STRING, -1);
		userCommandView.insertColumn("Command", G_TYPE_STRING, TreeView::STRING, -1);
		userCommandView.finalize();
		userCommandStore = gtk_list_store_newv(userCommandView.getColCount(), userCommandView.getGTypes());
		gtk_tree_view_set_model(userCommandView.get(), GTK_TREE_MODEL(userCommandStore));
		g_object_unref(userCommandStore);

		// Don't allow the columns to be sorted since we use move up/down functions 
		gtk_tree_view_column_set_sort_column_id(gtk_tree_view_get_column(userCommandView.get(), userCommandView.col("Name")), -1);
		gtk_tree_view_column_set_sort_column_id(gtk_tree_view_get_column(userCommandView.get(), userCommandView.col("Command")), -1);
		gtk_tree_view_column_set_sort_column_id(gtk_tree_view_get_column(userCommandView.get(), userCommandView.col("Hub")), -1);

		gtk_window_set_transient_for(GTK_WINDOW(getWidget("commandDialog")), GTK_WINDOW(getContainer()));

		g_signal_connect(getWidget("userCommandAddButton"), "clicked", G_CALLBACK(onUserCommandAdd_gui), (gpointer)this);
		g_signal_connect(getWidget("userCommandRemoveButton"), "clicked", G_CALLBACK(onUserCommandRemove_gui), (gpointer)this);
		g_signal_connect(getWidget("userCommandEditButton"), "clicked", G_CALLBACK(onUserCommandEdit_gui), (gpointer)this);
		g_signal_connect(getWidget("userCommandUpButton"), "clicked", G_CALLBACK(onUserCommandMoveUp_gui), (gpointer)this);
		g_signal_connect(getWidget("userCommandDownButton"), "clicked", G_CALLBACK(onUserCommandMoveDown_gui), (gpointer)this);
		g_signal_connect(getWidget("commandDialogSeparator"), "toggled", G_CALLBACK(onUserCommandTypeSeparator_gui), (gpointer)this);
		g_signal_connect(getWidget("commandDialogRaw"), "toggled", G_CALLBACK(onUserCommandTypeRaw_gui), (gpointer)this);
		g_signal_connect(getWidget("commandDialogChat"), "toggled", G_CALLBACK(onUserCommandTypeChat_gui), (gpointer)this);
		g_signal_connect(getWidget("commandDialogPM"), "toggled", G_CALLBACK(onUserCommandTypePM_gui), (gpointer)this);
		g_signal_connect(getWidget("commandDialogCommand"), "key-release-event", G_CALLBACK(onUserCommandKeyPress_gui), (gpointer)this);
		g_signal_connect(getWidget("commandDialogTo"), "key-release-event", G_CALLBACK(onUserCommandKeyPress_gui), (gpointer)this);
		loadUserCommands_gui();
	}

	{ // Experts
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("hashSpeedSpinButton")), (double)SETTING(MAX_HASH_SPEED));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("writeBufferSpinButton")), (double)SETTING(BUFFER_SIZE));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("pmHistorySpinButton")), (double)SETTING(SHOW_LAST_LINES_LOG));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("slotSizeSpinButton")), (double)SETTING(SET_MINISLOT_SIZE));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("maxListSizeSpinButton")), (double)SETTING(MAX_FILELIST_SIZE));
		gtk_entry_set_text(GTK_ENTRY(getWidget("CIDEntry")), SETTING(PRIVATE_ID).c_str());
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("autoRefreshSpinButton")), (double)SETTING(AUTO_REFRESH_TIME));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("searchHistorySpinButton")), (double)SETTING(SEARCH_HISTORY));
		gtk_entry_set_text(GTK_ENTRY(getWidget("bindAddressEntry")), SETTING(BIND_ADDRESS).c_str());
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("socketReadSpinButton")), (double)SETTING(SOCKET_IN_BUFFER));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(getWidget("socketWriteSpinButton")), (double)SETTING(SOCKET_OUT_BUFFER));
	}

	{ // Security Certificates
		gtk_entry_set_text(GTK_ENTRY(getWidget("privateKeyEntry")), SETTING(TLS_PRIVATE_KEY_FILE).c_str());
		gtk_entry_set_text(GTK_ENTRY(getWidget("certificateFileEntry")), SETTING(TLS_CERTIFICATE_FILE).c_str());
		gtk_entry_set_text(GTK_ENTRY(getWidget("trustedCertificatesPathEntry")), SETTING(TLS_TRUSTED_CERTIFICATES_PATH).c_str());
		g_signal_connect(getWidget("privateKeyButton"), "clicked", G_CALLBACK(onCertificatesPrivateBrowseClicked_gui), (gpointer)this);
		g_signal_connect(getWidget("certificateFileButton"), "clicked", G_CALLBACK(onCertificatesFileBrowseClicked_gui), (gpointer)this);
		g_signal_connect(getWidget("trustedCertificatesPathButton"), "clicked", G_CALLBACK(onCertificatesPathBrowseClicked_gui), (gpointer)this);

		createOptionsView_gui(certificatesView, certificatesStore, "certificatesTreeView");

		addOption_gui(certificatesStore, _("Use TLS when remote client supports it"), SettingsManager::USE_TLS);
		addOption_gui(certificatesStore, _("Allow TLS connections to hubs without trusted certificate"), SettingsManager::ALLOW_UNTRUSTED_HUBS);
		addOption_gui(certificatesStore, _("Allow TLS connections to clients without trusted certificate"), SettingsManager::ALLOW_UNTRUSTED_CLIENTS);

		g_signal_connect(getWidget("generateCertificatesButton"), "clicked", G_CALLBACK(onGenerateCertificatesClicked_gui), (gpointer)this);
	}
}

void Settings::onSoundFileBrowseClicked_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

	gint response = gtk_dialog_run(GTK_DIALOG(s->getWidget("fileChooserDialog")));
	gtk_widget_hide(s->getWidget("fileChooserDialog"));

	if (response == GTK_RESPONSE_OK)
	{
		gchar *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(s->getWidget("fileChooserDialog")));

		if (path)
		{
			GtkTreeIter iter;
			GtkTreeSelection *selection = gtk_tree_view_get_selection(s->soundView.get());

			if (gtk_tree_selection_get_selected(selection, NULL, &iter))
			{
				string sample = s->soundView.getString(&iter, "Sample");
				string target = Text::toUtf8(path);

				gtk_list_store_set(s->soundStore, &iter, s->soundView.col(_("File")), target.c_str(), -1);
			}
			g_free(path);
		}
	}
}

void Settings::onSoundPlayButton_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->soundView.get());

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		string target = s->soundView.getString(&iter, _("File"));
		Sound::get()->playSound(target);
	}
}

void Settings::onSoundNoneButton_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->soundView.get());

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		gtk_list_store_set(s->soundStore, &iter, s->soundView.col(_("File")), "", -1);
}

void Settings::onTextColorForeClicked_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	s->selectTextColor_gui(0);
}

void Settings::onTextColorBackClicked_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	s->selectTextColor_gui(1);
}

void Settings::onTextColorBWClicked_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	s->selectTextColor_gui(2);
}

void Settings::onTextStyleClicked_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	s->selectTextStyle_gui(0);
}

void Settings::onTextStyleDefaultClicked_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	s->selectTextStyle_gui(1);
}

void Settings::onPreviewAdd_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings*)data;

	string name = gtk_entry_get_text(GTK_ENTRY(s->getWidget("previewNameEntry")));
	string app = gtk_entry_get_text(GTK_ENTRY(s->getWidget("previewApplicationEntry")));
	string ext = gtk_entry_get_text(GTK_ENTRY(s->getWidget("previewExtensionsEntry")));

	if (name.empty() || app.empty() || ext.empty())
	{
		s->showErrorDialog(_("Name and command must not be empty"));
		return;
	}

	WulforSettingsManager *wsm = WulforSettingsManager::getInstance();

	if (wsm->getPreviewApp(name))
	{
		s->showErrorDialog(_("Add player failed"));
		return;
	}

	if (wsm->addPreviewApp(name, app, ext) != NULL)
	{
		GtkTreeIter it;
		gtk_list_store_append(s->previewAppToStore, &it);
		gtk_list_store_set(s->previewAppToStore, &it,
			s->previewAppView.col(_("Name")), name.c_str(),
			s->previewAppView.col(_("Application")), app.c_str(),
			s->previewAppView.col(_("Extensions")), ext.c_str(),
			-1);
	}
	else s->showErrorDialog(_(":("));
}

void Settings::onPreviewRemove_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->previewAppView.get());

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		string name = s->previewAppView.getString(&iter, _("Name"));

		if (WulforSettingsManager::getInstance()->removePreviewApp(name))
			gtk_list_store_remove(s->previewAppToStore, &iter);
	}
}

void Settings::onPreviewKeyReleased_gui(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	Settings *s = (Settings *)data;

	if (event->keyval == GDK_Up || event->keyval == GDK_Down)
	{
		GtkTreeIter iter;
		GtkTreeSelection *selection = gtk_tree_view_get_selection(s->previewAppView.get());

		if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		{
			gtk_entry_set_text(GTK_ENTRY(s->getWidget("previewNameEntry")), s->previewAppView.getString(&iter, _("Name")).c_str() );
			gtk_entry_set_text(GTK_ENTRY(s->getWidget("previewApplicationEntry")), s->previewAppView.getString(&iter, _("Application")).c_str());
			gtk_entry_set_text(GTK_ENTRY(s->getWidget("previewExtensionsEntry")), s->previewAppView.getString(&iter, _("Extensions")).c_str());
		}
	}
}

void Settings::onPreviewButtonReleased_gui(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	Settings *s = (Settings *)data;

	if (event->button == 3 || event->button == 1)
	{
		GtkTreeIter iter;
		GtkTreeSelection *selection = gtk_tree_view_get_selection(s->previewAppView.get());

		if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		{
			gtk_entry_set_text(GTK_ENTRY(s->getWidget("previewNameEntry")), s->previewAppView.getString(&iter, _("Name")).c_str());
			gtk_entry_set_text(GTK_ENTRY(s->getWidget("previewApplicationEntry")), s->previewAppView.getString(&iter, _("Application")).c_str());
			gtk_entry_set_text(GTK_ENTRY(s->getWidget("previewExtensionsEntry")), s->previewAppView.getString(&iter, _("Extensions")).c_str());
		}
	}
}

void Settings::onPreviewApply_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

	string name = gtk_entry_get_text(GTK_ENTRY(s->getWidget("previewNameEntry")));
	string app = gtk_entry_get_text(GTK_ENTRY(s->getWidget("previewApplicationEntry")));
	string ext = gtk_entry_get_text(GTK_ENTRY(s->getWidget("previewExtensionsEntry")));

	if (name.empty() || app.empty() || ext.empty())
	{
		s->showErrorDialog(_("Name and command must not be empty"));
		return;
	}

	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->previewAppView.get());

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		string oldName = s->previewAppView.getString(&iter, _("Name"));

		if (WulforSettingsManager::getInstance()->applyPreviewApp(oldName, name, app, ext))
		{
			gtk_list_store_set(s->previewAppToStore, &iter,
				s->previewAppView.col(_("Name")), name.c_str(),
				s->previewAppView.col(_("Application")), app.c_str(),
				s->previewAppView.col(_("Extensions")), ext.c_str(),
				-1);
		}
	}
}

void Settings::onAddShare_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

 	gint response = gtk_dialog_run(GTK_DIALOG(s->getWidget("dirChooserDialog")));
	gtk_widget_hide(s->getWidget("dirChooserDialog"));

	if (response == GTK_RESPONSE_OK)
	{
		gchar *temp = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(s->getWidget("dirChooserDialog")));
		if (temp)
		{
			string path = Text::toUtf8(temp);
			g_free(temp);

			if (path[path.length() - 1] != PATH_SEPARATOR)
				path += PATH_SEPARATOR;

			gtk_entry_set_text(GTK_ENTRY(s->getWidget("virtualNameDialogEntry")), Util::getLastDir(path).c_str());
			gtk_editable_select_region(GTK_EDITABLE(s->getWidget("virtualNameDialogEntry")), 0, -1);
			response = gtk_dialog_run(GTK_DIALOG(s->getWidget("virtualNameDialog")));
			gtk_widget_hide(s->getWidget("virtualNameDialog"));

			if (response == GTK_RESPONSE_OK)
			{
				string name = gtk_entry_get_text(GTK_ENTRY(s->getWidget("virtualNameDialogEntry")));
				typedef Func2<Settings, string, string> F2;
				F2 *func = new F2(s, &Settings::addShare_client, path, name);
				WulforManager::get()->dispatchClientFunc(func);
			}
		}
	}
}

void Settings::selectTextColor_gui(const int select)
{
	GtkTreeIter iter;

	if (select == 2)
	{
		/* black and white style */
		GtkTreeModel *m = GTK_TREE_MODEL(textStyleStore);
		gboolean valid = gtk_tree_model_get_iter_first(m, &iter);

		string style = "";
		GtkTextTag *tag = NULL;
		GtkTextTagTable *tag_table = gtk_text_buffer_get_tag_table(textStyleBuffer);

		while (valid)
		{
			gtk_list_store_set(textStyleStore, &iter,
				textStyleView.col("ForeColor"), "#000000",
				textStyleView.col("BackColor"), "#FFFFFF", -1);

			style = textStyleView.getString(&iter, "Style");
			tag = gtk_text_tag_table_lookup(tag_table, style.c_str());

			if (tag)
				g_object_set(tag, "foreground", "#000000", "background", "#FFFFFF", NULL);

			valid = gtk_tree_model_iter_next(m, &iter);
		}

		gtk_widget_queue_draw(getWidget("textViewPreviewStyles"));

		return;
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(textStyleView.get());

	if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		showErrorDialog(_("selected style failed"));
		return;
	}

	GdkColor color;
	string currentcolor = "";
	GtkColorSelection *colorsel = GTK_COLOR_SELECTION(getWidget("colorsel-color_selection"));

	if (select == 0)
		currentcolor = textStyleView.getString(&iter, "ForeColor");
	else
		currentcolor = textStyleView.getString(&iter, "BackColor");

	if (gdk_color_parse(currentcolor.c_str(), &color))
		gtk_color_selection_set_current_color(colorsel, &color);

	gint response = gtk_dialog_run(GTK_DIALOG(getWidget("colorSelectionDialog")));
	gtk_widget_hide(getWidget("colorSelectionDialog"));

	if (response == GTK_RESPONSE_OK)
	{
		gtk_color_selection_get_current_color(colorsel, &color);

		string ground = "";
		string strcolor = WulforUtil::colorToString(&color);
		string style = textStyleView.getString(&iter, "Style");

		if (select == 0)
		{
			ground = "foreground-gdk";
			gtk_list_store_set(textStyleStore, &iter, textStyleView.col("ForeColor"), strcolor.c_str(), -1);
		}
		else
		{
			ground = "background-gdk";
			gtk_list_store_set(textStyleStore, &iter, textStyleView.col("BackColor"), strcolor.c_str(), -1);
		}

		GtkTextTag *tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textStyleBuffer), style.c_str());

		if (tag)
			g_object_set(tag, ground.c_str(), &color, NULL);

		gtk_widget_queue_draw(getWidget("textViewPreviewStyles"));
	}
}

void Settings::selectTextStyle_gui(const int select)
{
	GtkTreeIter iter;
	int bolt, italic;
	GtkTextTag *tag = NULL;
	string style = "";

	WulforSettingsManager *wsm = WulforSettingsManager::getInstance();

	if (select == 1)
	{
		/* default style */
		GtkTreeModel *m = GTK_TREE_MODEL(textStyleStore);
		gboolean valid = gtk_tree_model_get_iter_first(m, &iter);
		GtkTextTagTable *tag_table = gtk_text_buffer_get_tag_table(textStyleBuffer);
		string fore, back;

		while (valid)
		{
			style = textStyleView.getString(&iter, "Style");
			fore = wsm->getString(textStyleView.getString(&iter, "keyForeColor"), true);
			back = wsm->getString(textStyleView.getString(&iter, "keyBackColor"), true);
			bolt = wsm->getInt(textStyleView.getString(&iter, "keyBolt"), true);
			italic = wsm->getInt(textStyleView.getString(&iter, "keyItalic"), true);

			gtk_list_store_set(textStyleStore, &iter,
				textStyleView.col("ForeColor"), fore.c_str(),
				textStyleView.col("BackColor"), back.c_str(),
				textStyleView.col("Bolt"), bolt,
				textStyleView.col("Italic"), italic,
				-1);

			tag = gtk_text_tag_table_lookup(tag_table, style.c_str());

			if (tag)
				g_object_set(tag,
					"foreground", fore.c_str(),
					"background", back.c_str(),
					"weight", (PangoWeight)bolt,
					"style", (PangoStyle)italic,
					NULL);

			valid = gtk_tree_model_iter_next(m, &iter);
		}

		gtk_widget_queue_draw(getWidget("textViewPreviewStyles"));

		return;
	}

	GtkTreeSelection *selection = gtk_tree_view_get_selection(textStyleView.get());

	if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		showErrorDialog(_("selected style failed"));
		return;
	}

	gint response = gtk_dialog_run(GTK_DIALOG(getWidget("fontSelectionDialog")));
	gtk_widget_hide(getWidget("fontSelectionDialog"));

	if (response == GTK_RESPONSE_OK)
	{
		GtkFontSelection *fontsel = GTK_FONT_SELECTION(getWidget("fontsel-font_selection"));
		gchar *temp = gtk_font_selection_get_font_name(fontsel);

		if (temp)
		{
			string font_name = temp;

			font_name.find("Bold") != string::npos ? bolt = TEXT_WEIGHT_BOLD: bolt = TEXT_WEIGHT_NORMAL;
			font_name.find("Italic") != string::npos ? italic = TEXT_STYLE_ITALIC: italic = TEXT_STYLE_NORMAL;

			style = textStyleView.getString(&iter, "Style");
			gtk_list_store_set(textStyleStore, &iter, textStyleView.col("Bolt"), bolt, textStyleView.col("Italic"), italic, -1);

			tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(textStyleBuffer), style.c_str());

			if (tag)
				g_object_set(tag, "weight", (PangoWeight)bolt, "style", (PangoStyle)italic, NULL);

			g_free(temp);

			gtk_widget_queue_draw(getWidget("textViewPreviewStyles"));
		}
	}
}

void Settings::loadUserCommands_gui()
{
	GtkTreeIter iter;
	gtk_list_store_clear(userCommandStore);

	UserCommand::List userCommands = FavoriteManager::getInstance()->getUserCommands();

	for (UserCommand::List::iterator i = userCommands.begin(); i != userCommands.end(); ++i)
	{
		UserCommand &uc = *i;
		if (!uc.isSet(UserCommand::FLAG_NOSAVE))
		{
			gtk_list_store_append(userCommandStore, &iter);
			gtk_list_store_set(userCommandStore, &iter,
				userCommandView.col("Name"), uc.getName().c_str(),
				userCommandView.col("Hub"), uc.getHub().c_str(),
				userCommandView.col("Command"), uc.getCommand().c_str(),
				-1);
		}
	}
}

void Settings::saveUserCommand(UserCommand *uc)
{
	string name, command, hub;
	int ctx = 0;
	int type = 0;
	GtkTreeIter iter;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("commandDialogHubMenu"))))
		ctx |= UserCommand::CONTEXT_HUB;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("commandDialogUserMenu"))))
		ctx |= UserCommand::CONTEXT_CHAT;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("commandDialogSearchMenu"))))
		ctx |= UserCommand::CONTEXT_SEARCH;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("commandDialogFilelistMenu"))))
		ctx |= UserCommand::CONTEXT_FILELIST;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("commandDialogSeparator"))))
	{
		name = _("Separator");
		type = UserCommand::TYPE_SEPARATOR;
	}
	else
	{
		name = gtk_entry_get_text(GTK_ENTRY(getWidget("commandDialogName")));
		command = gtk_entry_get_text(GTK_ENTRY(getWidget("commandDialogCommand")));
		hub = gtk_entry_get_text(GTK_ENTRY(getWidget("commandDialogHub")));

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("commandDialogChat"))))
		{
			command = "<%[myNI]> " + NmdcHub::validateMessage(command, FALSE) + "|";
		}
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("commandDialogPM"))))
		{
			string to = gtk_entry_get_text(GTK_ENTRY(getWidget("commandDialogTo")));
			if (to.length() == 0)
				to = "%[userNI]";

			command = "$To: " + to + " From: %[myNI] $<%[myNI]> " + NmdcHub::validateMessage(command, FALSE) + "|";
		}

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("commandDialogOnce"))))
			type = UserCommand::TYPE_RAW_ONCE;
		else
			type = UserCommand::TYPE_RAW;
	}

	if (uc == NULL)
	{
		FavoriteManager::getInstance()->addUserCommand(type, ctx, 0, name, command, hub);
		gtk_list_store_append(userCommandStore, &iter);
	}
	else
	{
		uc->setType(type);
		uc->setCtx(ctx);
		uc->setName(name);
		uc->setCommand(command);
		uc->setHub(hub);
		FavoriteManager::getInstance()->updateUserCommand(*uc);

		GtkTreeSelection *selection = gtk_tree_view_get_selection(userCommandView.get());
		if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
			return;
	}

	gtk_list_store_set(userCommandStore, &iter,
		userCommandView.col("Name"), name.c_str(),
		userCommandView.col("Hub"), hub.c_str(),
		userCommandView.col("Command"), command.c_str(),
		-1);
}


void Settings::updateUserCommandTextSent_gui()
{
	string command = gtk_entry_get_text(GTK_ENTRY(getWidget("commandDialogCommand")));

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("commandDialogSeparator"))))
	{
		command.clear();
	}
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("commandDialogChat"))))
	{
		command = "<%[myNI]> " + NmdcHub::validateMessage(command, FALSE) + "|";
	}
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("commandDialogPM"))))
	{
		string to = gtk_entry_get_text(GTK_ENTRY(getWidget("commandDialogTo")));
		if (to.length() == 0)
			to = "%[userNI]";
		command = "$To: " + to + " From: %[myNI] $<%[myNI]> " + NmdcHub::validateMessage(command, FALSE) + "|";
	}

	gtk_entry_set_text(GTK_ENTRY(getWidget("commandDialogTextSent")), command.c_str());
}

bool Settings::validateUserCommandInput(const string &oldName)
{
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(getWidget("commandDialogSeparator"))))
	{
		string name = gtk_entry_get_text(GTK_ENTRY(getWidget("commandDialogName")));
		string command = gtk_entry_get_text(GTK_ENTRY(getWidget("commandDialogCommand")));
		string hub = gtk_entry_get_text(GTK_ENTRY(getWidget("commandDialogHub")));

		if (name.length() == 0 || command.length() == 0)
		{
			showErrorDialog(_("Name and command must not be empty"));
			return FALSE;
		}

		if (name != oldName && FavoriteManager::getInstance()->findUserCommand(name, hub) != -1)
		{
			showErrorDialog(_("Command name already exists"));
			return FALSE;
		}
	}

	return TRUE;
}

void Settings::showErrorDialog(const string &error)
{
	GtkWidget *errorDialog = gtk_message_dialog_new(GTK_WINDOW(getWidget("dialog")),
		GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", error.c_str());
	gtk_dialog_run(GTK_DIALOG(errorDialog));
	gtk_widget_destroy(errorDialog);
}

void Settings::onOptionsViewToggled_gui(GtkCellRendererToggle *cell, gchar *path, gpointer data)
{
	GtkTreeIter iter;
	GtkListStore *store = (GtkListStore *)data;
	GtkTreeModel *model = GTK_TREE_MODEL(store);

	if (gtk_tree_model_get_iter_from_string(model, &iter, path))
	{
		gboolean fixed;
		gtk_tree_model_get(model, &iter, 0, &fixed, -1);
		gtk_list_store_set(store, &iter, 0, !fixed, -1);
	}
}

void Settings::onInDirect_gui(GtkToggleButton *button, gpointer data)
{
	Settings *s = (Settings *)data;
	gtk_widget_set_sensitive(s->getWidget("ipEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("ipLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("tcpEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("tcpLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("udpEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("udpLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("tlsEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("tlsLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("forceIPCheckButton"), TRUE);
}

/**@todo Uncomment when implemented
void Settings::onInFW_UPnP_gui(GtkToggleButton *button, gpointer data)
{
	Settings *s = (Settings *)data;
	gtk_widget_set_sensitive(s->getWidget("ipEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("ipLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("tcpEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("tcpLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("udpEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("udpLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("tlsEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("tlsEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("forceIPCheckButton"), TRUE);
}
*/

void Settings::onInFW_NAT_gui(GtkToggleButton *button, gpointer data)
{
	Settings *s = (Settings *)data;
	gtk_widget_set_sensitive(s->getWidget("ipEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("ipLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("tcpEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("tcpLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("udpEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("udpLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("tlsEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("tlsLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("forceIPCheckButton"), TRUE);
}

void Settings::onInPassive_gui(GtkToggleButton *button, gpointer data)
{
	Settings *s = (Settings *)data;
	gtk_widget_set_sensitive(s->getWidget("ipEntry"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("ipLabel"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("tcpEntry"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("tcpLabel"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("udpEntry"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("udpLabel"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("tlsEntry"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("tlsLabel"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("forceIPCheckButton"), FALSE);
}

void Settings::onOutDirect_gui(GtkToggleButton *button, gpointer data)
{
	Settings *s = (Settings *)data;
	gtk_widget_set_sensitive(s->getWidget("socksIPEntry"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("socksIPLabel"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("socksUserEntry"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("socksUserLabel"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("socksPortEntry"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("socksPortLabel"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("socksPassEntry"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("socksPassLabel"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("socksCheckButton"), FALSE);
}

void Settings::onSocks5_gui(GtkToggleButton *button, gpointer data)
{
	Settings *s = (Settings *)data;
	gtk_widget_set_sensitive(s->getWidget("socksIPEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("socksIPLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("socksUserEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("socksUserLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("socksPortEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("socksPortLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("socksPassEntry"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("socksPassLabel"), TRUE);
	gtk_widget_set_sensitive(s->getWidget("socksCheckButton"), TRUE);
}

void Settings::onBrowseFinished_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

 	gint response = gtk_dialog_run(GTK_DIALOG(s->getWidget("dirChooserDialog")));
	gtk_widget_hide(s->getWidget("dirChooserDialog"));

	if (response == GTK_RESPONSE_OK)
	{
		gchar *path = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(s->getWidget("dirChooserDialog")));
		if (path)
		{
			gtk_entry_set_text(GTK_ENTRY(s->getWidget("finishedDownloadsEntry")), Text::toUtf8(path).c_str());
			g_free(path);
		}
	}
}

void Settings::onBrowseUnfinished_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

 	gint response = gtk_dialog_run(GTK_DIALOG(s->getWidget("dirChooserDialog")));
	gtk_widget_hide(s->getWidget("dirChooserDialog"));

	if (response == GTK_RESPONSE_OK)
	{
		gchar *path = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(s->getWidget("dirChooserDialog")));
		if (path)
		{
			gtk_entry_set_text(GTK_ENTRY(s->getWidget("unfinishedDownloadsEntry")),  Text::toUtf8(path).c_str());
			g_free(path);
		}
	}
}

void Settings::onPublicHubs_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter iter;

	gtk_list_store_clear(s->publicListStore);
	StringList lists(FavoriteManager::getInstance()->getHubLists());
	for (StringList::iterator idx = lists.begin(); idx != lists.end(); ++idx)
	{
		gtk_list_store_append(s->publicListStore, &iter);
		gtk_list_store_set(s->publicListStore, &iter, s->publicListView.col("List"), (*idx).c_str(), -1);
	}

	gint response = gtk_dialog_run(GTK_DIALOG(s->getWidget("publicHubsDialog")));
	gtk_widget_hide(s->getWidget("publicHubsDialog"));

	if (response == GTK_RESPONSE_OK)
	{
		string lists = "";
		GtkTreeModel *m = GTK_TREE_MODEL(s->publicListStore);
		gboolean valid = gtk_tree_model_get_iter_first(m, &iter);
		while (valid)
		{
			lists += s->publicListView.getString(&iter, "List") + ";";
			valid = gtk_tree_model_iter_next(m, &iter);
		}
		if (!lists.empty())
			lists.erase(lists.size() - 1);
		SettingsManager::getInstance()->set(SettingsManager::HUBLIST_SERVERS, lists);
	}
}

void Settings::onPublicAdd_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *col;

	gtk_list_store_append(s->publicListStore, &iter);
	gtk_list_store_set(s->publicListStore, &iter, s->publicListView.col("List"), _("New list"), -1);
	path = gtk_tree_model_get_path(GTK_TREE_MODEL(s->publicListStore), &iter);
	col = gtk_tree_view_get_column(s->publicListView.get(), 0);
	gtk_tree_view_set_cursor(s->publicListView.get(), path, col, TRUE);
	gtk_tree_path_free(path);
}

void Settings::onPublicMoveUp_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter prev, current;
	GtkTreeModel *m = GTK_TREE_MODEL(s->publicListStore);
	GtkTreeSelection *sel = gtk_tree_view_get_selection(s->publicListView.get());

	if (gtk_tree_selection_get_selected(sel, NULL, &current))
	{
		GtkTreePath *path = gtk_tree_model_get_path(m, &current);
		if (gtk_tree_path_prev(path) && gtk_tree_model_get_iter(m, &prev, path))
			gtk_list_store_swap(s->publicListStore, &current, &prev);
		gtk_tree_path_free(path);
	}
}

void Settings::onPublicMoveDown_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter current, next;
	GtkTreeSelection *sel = gtk_tree_view_get_selection(s->publicListView.get());

	if (gtk_tree_selection_get_selected(sel, NULL, &current))
	{
		next = current;
		if (gtk_tree_model_iter_next(GTK_TREE_MODEL(s->publicListStore), &next))
			gtk_list_store_swap(s->publicListStore, &current, &next);
	}
}

void Settings::onPublicEdit_gui(GtkCellRendererText *cell, char *path, char *text, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(s->publicListStore), &iter, path))
		gtk_list_store_set(s->publicListStore, &iter, 0, text, -1);
}

void Settings::onPublicRemove_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->publicListView.get());

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		gtk_list_store_remove(s->publicListStore, &iter);
}

void Settings::onAddFavorite_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

 	gint response = gtk_dialog_run(GTK_DIALOG(s->getWidget("dirChooserDialog")));
	gtk_widget_hide(s->getWidget("dirChooserDialog"));

	if (response == GTK_RESPONSE_OK)
	{
		gchar *temp = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(s->getWidget("dirChooserDialog")));
		if (temp)
		{
			string path = Text::toUtf8(temp);
			g_free(temp);

			gtk_entry_set_text(GTK_ENTRY(s->getWidget("favoriteNameDialogEntry")), "");
			response = gtk_dialog_run(GTK_DIALOG(s->getWidget("favoriteNameDialog")));
			gtk_widget_hide(s->getWidget("favoriteNameDialog"));

			if (response == GTK_RESPONSE_OK)
			{
				string name = gtk_entry_get_text(GTK_ENTRY(s->getWidget("favoriteNameDialogEntry")));
				if (path[path.length() - 1] != PATH_SEPARATOR)
					path += PATH_SEPARATOR;

				if (!name.empty() && FavoriteManager::getInstance()->addFavoriteDir(path, name))
				{
					GtkTreeIter iter;
					gtk_list_store_append(s->downloadToStore, &iter);
					gtk_list_store_set(s->downloadToStore, &iter,
						s->downloadToView.col("Favorite Name"), name.c_str(),
						s->downloadToView.col("Directory"), path.c_str(),
						-1);
				}
				else
				{
					s->showErrorDialog(_("Directory or favorite name already exists"));
				}
			}
		}
	}
}

void Settings::onRemoveFavorite_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->downloadToView.get());

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		string path = s->downloadToView.getString(&iter, "Directory");
		if (FavoriteManager::getInstance()->removeFavoriteDir(path))
		{
			gtk_list_store_remove(s->downloadToStore, &iter);
			gtk_widget_set_sensitive(s->getWidget("favoriteRemoveButton"), FALSE);
		}
	}
}

gboolean Settings::onFavoriteButtonReleased_gui(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->downloadToView.get());

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		gtk_widget_set_sensitive(s->getWidget("favoriteRemoveButton"), TRUE);
	else
		gtk_widget_set_sensitive(s->getWidget("favoriteRemoveButton"), FALSE);

	return FALSE;
}

void Settings::addShare_gui(string path, string name, string error)
{
	if (error.empty())
	{
		GtkTreeIter iter;
		int64_t size = ShareManager::getInstance()->getShareSize(path);

		gtk_list_store_append(shareStore, &iter);
		gtk_list_store_set(shareStore, &iter,
			shareView.col("Virtual Name"), name.c_str(),
			shareView.col("Directory"), path.c_str(),
			shareView.col("Size"), Util::formatBytes(size).c_str(),
			shareView.col("Real Size"), size,
			-1);
	}
	else
	{
		showErrorDialog(error.c_str());
	}
}

void Settings::onRemoveShare_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->shareView.get());

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		string path = s->shareView.getString(&iter, "Directory");
		gtk_list_store_remove(s->shareStore, &iter);
		gtk_widget_set_sensitive(s->getWidget("sharedRemoveButton"), FALSE);

		ShareManager::getInstance()->removeDirectory(path);
	}
}

gboolean Settings::onShareButtonReleased_gui(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->shareView.get());

	if (gtk_tree_selection_count_selected_rows(selection) == 0)
		gtk_widget_set_sensitive(s->getWidget("sharedRemoveButton"), FALSE);
	else
		gtk_widget_set_sensitive(s->getWidget("sharedRemoveButton"), TRUE);

	return FALSE;
}

gboolean Settings::onShareHiddenPressed_gui(GtkToggleButton *togglebutton, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter iter;
	bool show;
	int64_t size;

	show = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->getWidget("shareHiddenCheckButton")));

	Func1<Settings, bool> *func = new Func1<Settings, bool>(s, &Settings::shareHidden_client, show);
	WulforManager::get()->dispatchClientFunc(func);

	gtk_list_store_clear(s->shareStore);
	StringPairList directories = ShareManager::getInstance()->getDirectories();
	for (StringPairList::iterator it = directories.begin(); it != directories.end(); ++it)
	{
		size = ShareManager::getInstance()->getShareSize(it->second);
		gtk_list_store_append(s->shareStore, &iter);
		gtk_list_store_set(s->shareStore, &iter,
			s->shareView.col("Virtual Name"), it->first.c_str(),
			s->shareView.col("Directory"), it->second.c_str(),
			s->shareView.col("Size"), Util::formatBytes(size).c_str(),
			s->shareView.col("Real Size"), size,
			-1);
	}

	string text = _("Total size: ") + Util::formatBytes(ShareManager::getInstance()->getShareSize());
	gtk_label_set_text(GTK_LABEL(s->getWidget("sharedSizeLabel")), text.c_str());

	return FALSE;
}

void Settings::onLogBrowseClicked_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

 	gint response = gtk_dialog_run(GTK_DIALOG(s->getWidget("dirChooserDialog")));
	gtk_widget_hide(s->getWidget("dirChooserDialog"));

	if (response == GTK_RESPONSE_OK)
	{
		gchar *path = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(s->getWidget("dirChooserDialog")));
		if (path)
		{
			gtk_entry_set_text(GTK_ENTRY(s->getWidget("logDirectoryEntry")), Text::toUtf8(path).c_str());
			g_free(path);
		}
	}
}

void Settings::onLogMainClicked_gui(GtkToggleButton *button, gpointer data)
{
	Settings *s = (Settings *)data;
	bool toggled = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(s->getWidget("logMainLabel"), toggled);
	gtk_widget_set_sensitive(s->getWidget("logMainEntry"), toggled);
}

void Settings::onLogPrivateClicked_gui(GtkToggleButton *button, gpointer data)
{
	Settings *s = (Settings *)data;
	bool toggled = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(s->getWidget("logPrivateLabel"), toggled);
	gtk_widget_set_sensitive(s->getWidget("logPrivateEntry"), toggled);
}

void Settings::onLogDownloadClicked_gui(GtkToggleButton *button, gpointer data)
{
	Settings *s = (Settings *)data;
	bool toggled = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(s->getWidget("logDownloadsLabel"), toggled);
	gtk_widget_set_sensitive(s->getWidget("logDownloadsEntry"), toggled);
}

void Settings::onLogUploadClicked_gui(GtkToggleButton *button, gpointer data)
{
	Settings *s = (Settings *)data;
	bool toggled = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(s->getWidget("logUploadsLabel"), toggled);
	gtk_widget_set_sensitive(s->getWidget("logUploadsEntry"), toggled);
}

void Settings::onUserCommandAdd_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

	// Reset dialog to default
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogSeparator")), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogHubMenu")), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogUserMenu")), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogSearchMenu")), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogFilelistMenu")), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogOnce")), FALSE);
	gtk_entry_set_text(GTK_ENTRY(s->getWidget("commandDialogName")), "");
	gtk_entry_set_text(GTK_ENTRY(s->getWidget("commandDialogCommand")), "");
	gtk_entry_set_text(GTK_ENTRY(s->getWidget("commandDialogHub")), "");
	gtk_entry_set_text(GTK_ENTRY(s->getWidget("commandDialogTo")), "");
	gtk_widget_set_sensitive(s->getWidget("commandDialogName"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("commandDialogCommand"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("commandDialogHub"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("commandDialogTo"), FALSE);
	gtk_widget_set_sensitive(s->getWidget("commandDialogOnce"), FALSE);

	gint response;

	do
	{
		response = gtk_dialog_run(GTK_DIALOG(s->getWidget("commandDialog")));
	}
	while (response == GTK_RESPONSE_OK && !s->validateUserCommandInput());

	gtk_widget_hide(s->getWidget("commandDialog"));

	if (response == GTK_RESPONSE_OK)
		s->saveUserCommand(NULL);
}

void Settings::onUserCommandEdit_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->userCommandView.get());

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		string name = s->userCommandView.getString(&iter, "Name");
		string hubStr = s->userCommandView.getString(&iter, "Hub");
		int cid = FavoriteManager::getInstance()->findUserCommand(name, hubStr);
		if (cid < 0)
			return;

		UserCommand uc;
		string command, nick;
		FavoriteManager::getInstance()->getUserCommand(cid, uc);
		bool hub = uc.getCtx() & UserCommand::CONTEXT_HUB;
		bool user = uc.getCtx() & UserCommand::CONTEXT_CHAT;
		bool search = uc.getCtx() & UserCommand::CONTEXT_SEARCH;
		bool filelist = uc.getCtx() & UserCommand::CONTEXT_FILELIST;

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogHubMenu")), hub);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogUserMenu")), user);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogSearchMenu")), search);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogFilelistMenu")), filelist);

		if (uc.getType() == UserCommand::TYPE_SEPARATOR)
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogSeparator")), TRUE);
		}
		else
		{
			command = uc.getCommand();
			bool once = uc.getType() == UserCommand::TYPE_RAW_ONCE;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogOnce")), once);

			// Chat Command
			if ((strncmp(command.c_str(), "<%[mynick]> ", 12) == 0 ||
				strncmp(command.c_str(), "<%[myNI]> ", 10) == 0) &&
				command.find('|') == command.length() - 1)
			{
				string::size_type i = command.find('>') + 2;
				command = NmdcHub::validateMessage(command.substr(i, command.length() - i - 1), TRUE);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogChat")), TRUE);
			}
			// PM command
			else if (strncmp(command.c_str(), "$To: ", 5) == 0 && (
				command.find(" From: %[myNI] $<%[myNI]> ") != string::npos ||
				command.find(" From: %[mynick] $<%[mynick]> ") != string::npos) && 
				command.find('|') == command.length() - 1)
			{
				string::size_type i = command.find(' ', 5);
				nick = command.substr(5, i - 5);
				i = command.find('>', 5) + 2;
				command = NmdcHub::validateMessage(command.substr(i, command.length() - i - 1), FALSE);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogPM")), TRUE);
			}
			// Raw command
			else
			{
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->getWidget("commandDialogRaw")), TRUE);
			}
		}

		gtk_entry_set_text(GTK_ENTRY(s->getWidget("commandDialogName")), name.c_str());
		gtk_entry_set_text(GTK_ENTRY(s->getWidget("commandDialogCommand")), command.c_str());
		gtk_entry_set_text(GTK_ENTRY(s->getWidget("commandDialogHub")), uc.getHub().c_str());
		gtk_entry_set_text(GTK_ENTRY(s->getWidget("commandDialogTo")), nick.c_str());

		s->updateUserCommandTextSent_gui();

		gint response;

		do
		{
			response = gtk_dialog_run(GTK_DIALOG(s->getWidget("commandDialog")));
		}
		while (response == GTK_RESPONSE_OK && !s->validateUserCommandInput(name));

		gtk_widget_hide(s->getWidget("commandDialog"));

		if (response == GTK_RESPONSE_OK)
			s->saveUserCommand(&uc);
	}
}

void Settings::onUserCommandMoveUp_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter prev, current;
	GtkTreeModel *m = GTK_TREE_MODEL(s->userCommandStore);
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->userCommandView.get());

	if (gtk_tree_selection_get_selected(selection, NULL, &current))
	{
		GtkTreePath *path = gtk_tree_model_get_path(m, &current);
		if (gtk_tree_path_prev(path) && gtk_tree_model_get_iter(m, &prev, path))
		{
			string name = s->userCommandView.getString(&current, "Name");
			string hub= s->userCommandView.getString(&current, "Hub");
			gtk_list_store_swap(s->userCommandStore, &current, &prev);

			typedef Func3<Settings, string, string, int> F3;
			F3 *func = new F3(s, &Settings::moveUserCommand_client, name, hub, -1);
			WulforManager::get()->dispatchClientFunc(func);
		}
		gtk_tree_path_free(path);
	}
}

void Settings::onUserCommandMoveDown_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter current, next;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->userCommandView.get());

	if (gtk_tree_selection_get_selected(selection, NULL, &current))
	{
		next = current;
		if (gtk_tree_model_iter_next(GTK_TREE_MODEL(s->userCommandStore), &next))
		{
			string name = s->userCommandView.getString(&current, "Name");
			string hub = s->userCommandView.getString(&current, "Hub");
			gtk_list_store_swap(s->userCommandStore, &current, &next);

			typedef Func3<Settings, string, string, int> F3;
			F3 *func = new F3(s, &Settings::moveUserCommand_client, name, hub, 1);
			WulforManager::get()->dispatchClientFunc(func);
		}
	}
}

void Settings::onUserCommandRemove_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(s->userCommandView.get());

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		string name = s->userCommandView.getString(&iter, "Name");
		string hub = s->userCommandView.getString(&iter, "Hub");
		gtk_list_store_remove(s->userCommandStore, &iter);

		typedef Func2<Settings, string, string> F2;
		F2 *func = new F2(s, &Settings::removeUserCommand_client, name, hub);
		WulforManager::get()->dispatchClientFunc(func);
	}
}

void Settings::onUserCommandTypeSeparator_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
	{
		gtk_widget_set_sensitive(s->getWidget("commandDialogName"), FALSE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogCommand"), FALSE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogHub"), FALSE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogTo"), FALSE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogOnce"), FALSE);

		s->updateUserCommandTextSent_gui();
	}
}

void Settings::onUserCommandTypeRaw_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
	{
		gtk_widget_set_sensitive(s->getWidget("commandDialogName"), TRUE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogCommand"), TRUE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogHub"), TRUE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogTo"), FALSE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogOnce"), TRUE);

		s->updateUserCommandTextSent_gui();
	}
}

void Settings::onUserCommandTypeChat_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
	{
		gtk_widget_set_sensitive(s->getWidget("commandDialogName"), TRUE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogCommand"), TRUE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogHub"), FALSE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogTo"), FALSE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogOnce"), TRUE);

		s->updateUserCommandTextSent_gui();
	}
}

void Settings::onUserCommandTypePM_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
	{
		gtk_widget_set_sensitive(s->getWidget("commandDialogName"), TRUE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogCommand"), TRUE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogHub"), TRUE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogTo"), TRUE);
		gtk_widget_set_sensitive(s->getWidget("commandDialogOnce"), TRUE);

		s->updateUserCommandTextSent_gui();
	}
}

gboolean Settings::onUserCommandKeyPress_gui(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	Settings *s = (Settings *)data;

	s->updateUserCommandTextSent_gui();

	return FALSE;
}

void Settings::onCertificatesPrivateBrowseClicked_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

 	gint response = gtk_dialog_run(GTK_DIALOG(s->getWidget("fileChooserDialog")));
	gtk_widget_hide(s->getWidget("fileChooserDialog"));

	if (response == GTK_RESPONSE_OK)
	{
		gchar *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(s->getWidget("fileChooserDialog")));
		if (path)
		{
			gtk_entry_set_text(GTK_ENTRY(s->getWidget("privateKeyEntry")), Text::toUtf8(path).c_str());
			g_free(path);
		}
	}
}

void Settings::onCertificatesFileBrowseClicked_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

 	gint response = gtk_dialog_run(GTK_DIALOG(s->getWidget("fileChooserDialog")));
	gtk_widget_hide(s->getWidget("fileChooserDialog"));

	if (response == GTK_RESPONSE_OK)
	{
		gchar *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(s->getWidget("fileChooserDialog")));
		if (path)
		{
			gtk_entry_set_text(GTK_ENTRY(s->getWidget("certificateFileEntry")), Text::toUtf8(path).c_str());
			g_free(path);
		}
	}
}

void Settings::onCertificatesPathBrowseClicked_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;

 	gint response = gtk_dialog_run(GTK_DIALOG(s->getWidget("dirChooserDialog")));
	gtk_widget_hide(s->getWidget("dirChooserDialog"));

	if (response == GTK_RESPONSE_OK)
	{
		gchar *path = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(s->getWidget("dirChooserDialog")));
		if (path)
		{
			gtk_entry_set_text(GTK_ENTRY(s->getWidget("trustedCertificatesPathEntry")), Text::toUtf8(path).c_str());
			g_free(path);
		}
	}
}

void Settings::onGenerateCertificatesClicked_gui(GtkWidget *widget, gpointer data)
{
	Settings *s = (Settings *)data;
	Func0<Settings> *func = new Func0<Settings>(s, &Settings::generateCertificates_client);
	WulforManager::get()->dispatchClientFunc(func);
}

void Settings::shareHidden_client(bool show)
{
	SettingsManager::getInstance()->set(SettingsManager::SHARE_HIDDEN, show);
	ShareManager::getInstance()->setDirty();
	ShareManager::getInstance()->refresh(TRUE, FALSE, TRUE);
}

void Settings::addShare_client(string path, string name)
{
	string error;

	try
	{
		ShareManager::getInstance()->addDirectory(path, name);
	}
	catch (const ShareException &e)
	{
		error = e.getError();
	}

	typedef Func3<Settings, string, string, string> F3;
	F3 *func = new F3(this, &Settings::addShare_gui, path, name, error);
	WulforManager::get()->dispatchGuiFunc(func);
}

void Settings::removeUserCommand_client(string name, string hub)
{
	if (!name.empty())
	{
		FavoriteManager *fm = FavoriteManager::getInstance();
		fm->removeUserCommand(fm->findUserCommand(name, hub));
	}
}

void Settings::moveUserCommand_client(string name, string hub, int pos)
{
	if (!name.empty())
	{
		FavoriteManager *fm = FavoriteManager::getInstance();
		fm->moveUserCommand(fm->findUserCommand(name, hub), pos);
	}
}

void Settings::generateCertificates_client()
{
	try
	{
		CryptoManager::getInstance()->generateCertificate();
	}
	catch (const CryptoException &e)
	{
	}
}
