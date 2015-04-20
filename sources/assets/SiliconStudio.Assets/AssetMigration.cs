﻿// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.

using System;
using System.Globalization;
using System.IO;
using System.Linq;
using SharpYaml;
using SharpYaml.Events;
using SharpYaml.Serialization;
using SiliconStudio.Assets.Serializers;
using SiliconStudio.Core.Diagnostics;
using SiliconStudio.Core.Yaml;

namespace SiliconStudio.Assets
{
    /// <summary>
    /// Helper for migrating asset to newer versions.
    /// </summary>
    static class AssetMigration
    {
        public static bool MigrateAssetIfNeeded(ILogger log, string assetFullPath)
        {
            // Determine if asset was Yaml or not
            var assetFileExtension = Path.GetExtension(assetFullPath);
            if (assetFileExtension == null)
                return false;

            assetFileExtension = assetFileExtension.ToLowerInvariant();

            var serializer = AssetSerializer.FindSerializer(assetFileExtension);
            if (!(serializer is AssetYamlSerializer))
                return false;

            // We've got a Yaml asset, let's get expected and serialized versions
            var serializedVersion = 0;
            int expectedVersion;
            Type assetType;

            // Read from Yaml file the asset version and its type (to get expected version)
            // Note: It tries to read as few as possible (SerializedVersion is expected to be right after Id, so it shouldn't try to read further than that)
            using (var streamReader = new StreamReader(assetFullPath))
            {
                var yamlEventReader = new EventReader(new Parser(streamReader));

                // Skip header
                yamlEventReader.Expect<StreamStart>();
                yamlEventReader.Expect<DocumentStart>();
                var mappingStart = yamlEventReader.Expect<MappingStart>();

                var yamlSerializerSettings = YamlSerializer.GetSerializerSettings();
                var tagTypeRegistry = yamlSerializerSettings.TagTypeRegistry;
                assetType = tagTypeRegistry.TypeFromTag(mappingStart.Tag);

                expectedVersion = AssetRegistry.GetFormatVersion(assetType);

                Scalar assetKey;
                while ((assetKey = yamlEventReader.Allow<Scalar>()) != null)
                {
                    // Only allow Id before SerializedVersion
                    if (assetKey.Value == "Id")
                    {
                        yamlEventReader.Skip();
                        continue;
                    }
                    if (assetKey.Value == "SerializedVersion")
                    {
                        serializedVersion = Convert.ToInt32(yamlEventReader.Expect<Scalar>().Value, CultureInfo.InvariantCulture);
                        break;
                    }
                }
            }

            if (serializedVersion > expectedVersion)
            {
                // Try to open an asset newer than what we support (probably generated by a newer Paradox)
                throw new InvalidOperationException(string.Format("Asset of type {0} has been serialized with newer version {1}, but only version {2} is supported. Was this asset created with a newer version of Paradox?", assetType, serializedVersion, expectedVersion));
            }

            if (serializedVersion < expectedVersion)
            {
                // Perform asset upgrade
                log.Verbose("{0} needs update, from version {1} to version {2}", Path.GetFullPath(assetFullPath), serializedVersion, expectedVersion);

                // Load the asset as a YamlNode object
                var input = new StringReader(File.ReadAllText(assetFullPath));
                var yamlStream = new YamlStream();
                yamlStream.Load(input);
                var yamlRootNode = (YamlMappingNode)yamlStream.Documents[0].RootNode;

                // Check if there is any asset updater
                var assetUpdaterTypes = AssetRegistry.GetFormatVersionUpdaterTypes(assetType);
                if (assetUpdaterTypes == null)
                {
                    throw new InvalidOperationException(string.Format("Asset of type {0} should be updated from version {1} to {2}, but no asset migration path was found", assetType, serializedVersion, expectedVersion));
                }

                // Instantiate asset updaters
                var assetUpgraders = assetUpdaterTypes.Select(x => (IAssetUpgrader)Activator.CreateInstance(x)).ToArray();

                // TODO: Select best asset updater if more than one (need to check from what to what version they update, score, if multiple need to be chained, etc...)
                // I think it's better to wait for some actual scenarios to implement this right the first time
                if (assetUpgraders.Length != 1)
                {
                    throw new InvalidOperationException(string.Format("Asset of type {0} has multiple migration paths, but selecting the right one is not implemented yet.", assetType));
                }

                // Perform upgrade
                assetUpgraders[0].Upgrade(log, yamlRootNode);

                // Make sure asset is updated to latest version
                YamlNode serializedVersionNode;
                var newSerializedVersion = 0;
                if (yamlRootNode.Children.TryGetValue(new YamlScalarNode("SerializedVersion"), out serializedVersionNode))
                {
                    newSerializedVersion = Convert.ToInt32(((YamlScalarNode)serializedVersionNode).Value);
                }

                if (newSerializedVersion != expectedVersion)
                {
                    throw new InvalidOperationException(string.Format("Asset of type {0} was migrated, but still its new version {1} doesn't match expected version {2}.", assetType, newSerializedVersion, expectedVersion));
                }

                log.Info("{0} updated from version {1} to version {2}", Path.GetFullPath(assetFullPath), serializedVersion, expectedVersion);

                var preferredIndent = YamlSerializer.GetSerializerSettings().PreferredIndent;

                // Save asset back to disk
                using (var streamWriter = new StreamWriter(assetFullPath))
                    yamlStream.Save(streamWriter, true, preferredIndent);

                return true;
            }

            return false;
        }
    }
}