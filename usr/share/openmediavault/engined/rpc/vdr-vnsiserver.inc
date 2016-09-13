<?php

/**
 * Copyright (C) 2016 OpenMediaVault Plugin Developers.
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

namespace OMV\Engined\Rpc;

use OMV\Config\Database;
use OMV\Engine\Notify\Dispatcher;
use OMV\ExecException;
use OMV\Rpc\ServiceAbstract;
use OMV\System\Process;

class VdrVnsiserver extends ServiceAbstract
{
    /** @var Database */
    private $database;

    /** @var string */
    private $dataModelPath = 'conf.service.vdr-vnsiserver';

    /** @var Dispatcher */
    private $dispatcher;

    /** @var string */
    private $vdrEventMessagePath = 'org.openmediavault.conf.service.vdr';

    /** @var string */
    private $vnsiserverReleasesUrl = 'https://api.github.com/repos/FernetMenta/vdr-plugin-vnsiserver/releases';

    public function __construct(Database $database = null, Dispatcher $dispatcher = null)
    {
        $this->database = $database ? $database : Database::getInstance();
        $this->dispatcher = $dispatcher ? $dispatcher : Dispatcher::getInstance();
    }

    /**
     * Get the name of the RPC service.
     *
     * @return string
     */
    public function getName()
    {
        return 'VdrVnsiserver';
    }

    /**
     * Initialize the RPC service.
     *
     * @return void
     */
    public function initialize()
    {
        $this->registerMethod('getSettings');
        $this->registerMethod('setSettings');
        $this->registerMethod('buildPlugin');
        $this->registerMethod('enumerateAvailablePluginReleases');
    }

    /**
     * Get the settings of the service.
     *
     * @param array $params  The method parameters.
     * @param array $context The context of the caller.
     *
     * @return array
     */
    public function getSettings($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);

        $object = $this->database->get($this->dataModelPath);

        $object->add('build_version', 'string');
        $object->set('build_version', $this->extractVnsiServerVersion());

        return $object->getAssoc();
    }

    /**
     * Set the settings of the service.
     *
     * @param array $params  The method parameters.
     * @param array $context The context of the caller.
     *
     * @return array
     */
    public function setSettings($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.vdr-vnsiserver.setsettings');

        $object = $this->database->get($this->dataModelPath);
        $object->setAssoc($params);

        $this->database->set($object);
        $this->dispatcher->notify(OMV_NOTIFY_MODIFY, $this->vdrEventMessagePath, $object);

        return $object->getAssoc();
    }

    /**
     * Build the VDR VNSI plugin from source. Expects an tarball_url parameter.
     *
     * @param array $params  The method parameters.
     * @param array $context The context of the caller.
     */
    public function buildPlugin($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.vdr-vnsiserver.buildplugin');

        $tarballUrl = $params['tarball_url'];

        // Create a background process.
        return $this->execBgProc(function ($bgStatusFilename, $bgOutputFilename) use ($tarballUrl) {
            $cmd = new Process('omv-vdr-build-vnsi', escapeshellarg($tarballUrl));
            $cmd->setRedirect1toFile($bgOutputFilename);
            $cmd->setRedirect2toFile($bgOutputFilename);
            $cmd->execute($output);

            return $output;
        });
    }

    /**
     * Enumerate available VNSI releases to build.
     *
     * @param array $params  The method parameters.
     * @param array $context The context of the caller.
     *
     * @return array
     */
    public function enumerateAvailablePluginReleases($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);

        // Github requires an user-agent:
        //   https://developer.github.com/v3/#user-agent-required
        $context = stream_context_create([
            'http' => ['user_agent' => 'OpenMediaVault VDR VNSI'],
        ]);
        $json = file_get_contents($this->vnsiserverReleasesUrl, false, $context);
        $data = json_decode($json);

        return $data;
    }

    /**
     * Get the current version of VNSI plugin.
     *
     * @return string The version number or n/a if not found.
     */
    private function extractVnsiServerVersion()
    {
        $output = [];
        $version = 'n/a';
        $versionRegex = '/^vnsiserver \(([\d\.]+)\)/';

        $cmd = new Process('vdr', '-V');

        try {
            $cmd->execute($output);
        } catch (ExecException $e) {
            return $version;
        }

        foreach ($output as $line) {
            $matches = [];

            preg_match($versionRegex, $line, $matches);

            if (!empty($matches)) {
                $version = $matches[1];
                break;
            }
        }

        return $version;
    }
}
