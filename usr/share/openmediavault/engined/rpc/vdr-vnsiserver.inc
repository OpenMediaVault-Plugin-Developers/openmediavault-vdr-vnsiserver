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
require_once 'openmediavault/config.inc';
require_once 'openmediavault/error.inc';
require_once 'openmediavault/notify.inc';
require_once 'openmediavault/rpcservice.inc';

class OMVRpcServiceVdrVnsiserver extends OMVRpcServiceAbstract
{
    /**
     * The main event message path for the VDR plugin.
     *
     * @var string
     */
    private $vdrEventMessagePath = 'org.openmediavault.services.vdr';

    /**
     * The API URL for fetching the release list for vdr-plugin-vnsiserver.
     *
     * @var string
     */
    private $vnsiserverReleasesUrl = 'https://api.github.com/repos/FernetMenta/vdr-plugin-vnsiserver/releases';

    /**
     * Get the service configuration XPath.
     *
     * @return string
     */
    private function getXpath()
    {
        return '/config/services/vdr-vnsiserver';
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
     * @return mixed
     *
     * @throws OMVException
     */
    public function getSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);

        // Get the configuration object.
        $object = $xmlConfig->get($this->getXpath());

        if (is_null($object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $xpath
            );
        }

        $object['build_version'] = $this->extractVnsiServerVersion();
        $object['enable'] = boolval($object['enable']);
        $object['allowed_hosts'] = $object['allowed_hosts'];

        return $object;
    }

    /**
     * Set the settings of the service.
     *
     * @param array $params  The method parameters.
     * @param array $context The context of the caller.
     *
     * @return mixed
     *
     * @throws OMVException
     */
    public function setSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "enable": {
                        "type": "boolean"
                    },
                    "allowed_hosts": {
                        "type": "string"
                    }
                }
            }'
        );

        $object = [
            'enable' => boolval($params['enable']),
            'allowed_hosts' => $params['allowed_hosts'],
        ];

        if ($xmlConfig->replace($this->getXpath(), $object) === false) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED);
        }

        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(OMV_NOTIFY_MODIFY, $this->vdrEventMessagePath, $object);

        return $object;
    }

    /**
     * Build the VDR VNSI plugin from source. Expects an tarball_url parameter.
     *
     * @param array $params  The method parameters.
     * @param array $context The context of the caller.
     *
     * @throws OMVException
     */
    public function buildPlugin($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "tarball_url": {
                        "type": "string"
                    }
                }
            }'
        );

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();

        // Parent process.
        if ($pid > 0) {
            $this->initializeBgProcStatus($bgStatusFilename, $pid);

            return $bgStatusFilename;
        }

        // Child process.
        try {
            $bgOutputFilename = $this->createBgProcOutput();

            $this->updateBgProcStatus(
                $bgStatusFilename,
                'outputfilename',
                $bgOutputFilename
            );

            $cmd = sprintf('export LANG=C; omv-vdr-build-vnsi \'%s\'', $params['tarball_url']);

            if ($this->exec($cmd, $output, $bgOutputFilename) !== 0) {
                throw new OMVException(OMVErrorMsg::E_EXEC_FAILED, $cmd, implode(PHP_EOL, $output));
            }

            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            exit(0);
        } catch (Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, '', $e);
            exit(1);
        }
    }

    /**
     * Enumerate available VNSI releases to build.
     *
     * @param array $params  The method parameters.
     * @param array $context The context of the caller.
     */
    public function enumerateAvailablePluginReleases($params, $context)
    {
        // Validate the RPC caller context.
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

        if ($this->exec('vdr -V', $output) === -1) {
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

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceVdrVnsiserver());
