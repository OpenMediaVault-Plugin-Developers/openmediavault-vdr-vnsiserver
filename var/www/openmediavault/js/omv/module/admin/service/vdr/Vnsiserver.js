/**
 * Copyright (C) 2016 OpenMediaVault Plugin Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// require("js/omv/WorkspaceManager.js")
// require("js/omv/workspace/form/Panel.js")
// require("js/omv/data/Model.js")
// require("js/omv/data/Store.js")
// require("js/omv/data/proxy/Rpc.js")
// require("js/omv/window/Execute.js")

Ext.define('OMV.module.admin.service.vdr.Vnsiserver', {
    extend: 'OMV.workspace.form.Panel',

    requires: [
        'OMV.data.proxy.Rpc',
        'OMV.data.Model',
        'OMV.data.Store',
        'OMV.window.Execute'
    ],

    rpcService: 'VdrVnsiserver',
    rpcGetMethod: 'getSettings',
    rpcSetMethod: 'setSettings',

    getFormItems: function() {
        return [{
            xtype: 'fieldset',
            title: _('Build information'),
            fieldDefaults: {
                labelSeparator: ''
            },
            items: [{
                xtype: 'displayfield',
                name: 'build_version',
                fieldLabel: _('Current version'),
            }, {
                xtype: 'fieldcontainer',
                layout: 'hbox',
                items: [{
                    xtype: 'combo',
                    name: 'new_version',
                    fieldLabel: _('New version'),
                    flex: 1,
                    displayField: 'tag_name',
                    editable: false,
                    emptyText: _('Select version to build ...'),
                    store: Ext.create('OMV.data.Store', {
                        autoLoad: true,
                        model: OMV.data.Model.createImplicit({
                            idProperty: 'id',
                            field: [{
                                name: 'id',
                                type: 'integer'
                            }, {
                                name: 'published_at',
                                type: 'date',
                                dateFormat: 'c'
                            }, {
                                name: 'tag_name',
                                type: 'string',
                            }, {
                                name: 'tarball_url',
                                type: 'string'
                            }]
                        }),
                        proxy: {
                            type: 'rpc',
                            rpcData: {
                                service: 'VdrVnsiserver',
                                method: 'enumerateAvailablePluginReleases'
                            },
                            appendSortParams: false
                        }
                    }),
                    submitValue: false,
                    triggerAction: 'all',
                    valueField: 'tarball_url'
                }, {
                    xtype: 'button',
                    text: _('Build'),
                    icon: 'images/wrench.png',
                    iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
                    scope: this,
                    handler: Ext.Function.bind(this.onBuildButton, this)
                }]
            }]
        }, {
            xtype: 'fieldset',
            title: _('General settings'),
            fieldDefaults: {
                labelSeparator: ''
            },
            items: [{
                xtype: 'checkbox',
                name: 'enable',
                fieldLabel: _('Enable'),
                checked: false
            }, {
                xtype: 'textfield',
                name: 'allowed_hosts',
                fieldLabel: _('Allowed hosts'),
                allowBlank: false
            }]
        }];
    },

    onBuildButton: function() {
        var field = this.findField('new_version');

        if (!field.getValue()) {
            field.markInvalid('No version to build selected.');
            return;
        }

        var wnd = Ext.create('OMV.window.Execute', {
            title: _('Building the VNSI plugin ...'),
            rpcService: 'VdrVnsiserver',
            rpcMethod: 'buildPlugin',
            rpcParams: {
                tarball_url: field.getValue()
            },
            rpcIgnoreErrors: false,
            hideStartButton: true,
            hideStopButton: true,
            listeners: {
                scope: this,
                finish: function(wnd, response) {
                    wnd.appendValue(_('Done ...'));
                },
                exception: function(wnd, error) {
                    OMV.MessageBox.error(null, error);
                }
            }
        });

        wnd.setButtonDisabled('close', true);
        wnd.show();
        wnd.start();
    }
});

OMV.WorkspaceManager.registerPanel({
    id: 'vnsiserver',
    path: '/service/vdr',
    text: _('VNSI'),
    position: 50,
    className: 'OMV.module.admin.service.vdr.Vnsiserver'
});
