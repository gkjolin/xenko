// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.
using System;
using System.Collections.Generic;
using SiliconStudio.Core;
using SiliconStudio.Core.Diagnostics;
using SiliconStudio.Core.IO;

namespace SiliconStudio.Assets.Templates
{
    public sealed class SessionTemplateGeneratorParameters : TemplateGeneratorParameters
    {
        /// <summary>
        /// Gets or sets the current session.
        /// </summary>
        /// <value>The session.</value>
        public PackageSession Session { get; set; }

        protected override void ValidateParameters()
        {
            base.ValidateParameters();

            if (Description.Scope != TemplateScope.Session)
            {
                throw new InvalidOperationException($"[{nameof(Description)}.{nameof(Description.Scope)}] must be {TemplateScope.Session} in {GetType().Name}");
            }
            if (Session == null)
            {
                throw new InvalidOperationException($"[{nameof(Session)}] cannot be null in {GetType().Name}");
            }
        }
    }

    public class PackageTemplateGeneratorParameters : TemplateGeneratorParameters
    {
        public PackageTemplateGeneratorParameters()
        {
        }

        public PackageTemplateGeneratorParameters(TemplateGeneratorParameters parameters, Package package)
            : base(parameters)
        {
            Package = package;
        }
        /// <summary>
        /// Gets or sets the package in which to execute this template
        /// </summary>
        /// <value>The package.</value>
        public Package Package { get; set; }

        protected override void ValidateParameters()
        {
            base.ValidateParameters();

            if (Description.Scope != TemplateScope.Package && GetType() == typeof(PackageTemplateGeneratorParameters))
            {
                throw new InvalidOperationException($"[{nameof(Description)}.{nameof(Description.Scope)}] must be {TemplateScope.Package} in {GetType().Name}");
            }
            if (Package == null)
            {
                throw new InvalidOperationException($"[{nameof(Package)}] cannot be null in {GetType().Name}");
            }
        }
    }

    public class AssetTemplateGeneratorParameters : PackageTemplateGeneratorParameters
    {
        public AssetTemplateGeneratorParameters(UDirectory targetLocation, IEnumerable<UFile> sourceFiles = null)
        {
            TargetLocation = targetLocation;
            SourceFiles = new List<UFile>();
            if (sourceFiles != null)
                SourceFiles.AddRange(sourceFiles);
        }

        public UDirectory TargetLocation { get; }

        public List<UFile> SourceFiles { get; }

        protected override void ValidateParameters()
        {
            base.ValidateParameters();

            if (Description.Scope != TemplateScope.Asset)
            {
                throw new InvalidOperationException($"[{nameof(Description)}.{nameof(Description.Scope)}] must be {TemplateScope.Asset} in {GetType().Name}");
            }
            if (Package == null)
            {
                throw new InvalidOperationException($"[{nameof(Package)}] cannot be null in {GetType().Name}");
            }
        }
    }

    /// <summary>
    /// Parameters used by <see cref="ITemplateGenerator{TParameters}"/>
    /// </summary>
    public abstract class TemplateGeneratorParameters
    {
        protected TemplateGeneratorParameters()
        {
        }

        protected TemplateGeneratorParameters(TemplateGeneratorParameters parameters)
        {
            Name = parameters.Name;
            Namespace = parameters.Namespace;
            OutputDirectory = parameters.OutputDirectory;
            Description = parameters.Description;
            Logger = parameters.Logger;
            parameters.Tags.CopyTo(ref Tags);
        }

        /// <summary>
        /// Gets or sets the project name used to generate the template.
        /// </summary>
        /// <value>The name.</value>
        public string Name { get; set; }

        /// <summary>
        /// Gets or sets the default namespace of this project.
        /// </summary>
        /// <value> The namespace. </value>
        public string Namespace { get; set; }

        /// <summary>
        /// Gets or sets the output directory.
        /// </summary>
        /// <value>The output directory.</value>
        public UDirectory OutputDirectory { get; set; }

        /// <summary>
        /// The actual template description.
        /// </summary>
        public TemplateDescription Description { get; set; }

        /// <summary>
        /// Gets or sets the logger.
        /// </summary>
        /// <value>The logger.</value>
        public LoggerResult Logger { get; set; }

        /// <summary>
        /// Contains extra properties that can be consumed by template generators.
        /// </summary>
        public PropertyContainer Tags;

        /// <summary>
        /// Validates this instance (all fields must be setup)
        /// </summary>
        public void Validate()
        {
            ValidateParameters();
        }

        /// <summary>
        /// Gets the tag corresponding to the given property key.
        /// </summary>
        /// <typeparam name="T">The generic type of the property key.</typeparam>
        /// <param name="key">The property key for which to retrieve the value.</param>
        /// <returns>The value of the tag corresponding to the given property key.</returns>
        /// <exception cref="KeyNotFoundException">Tag not found in template generator parameters.</exception>
        public T GetTag<T>(PropertyKey<T> key)
        {
            T result;
            if (!Tags.TryGetValue(key, out result))
            {
                throw new KeyNotFoundException("Tag not found in template generator parameters");
            }
            return result;
        }

        /// <summary>
        /// Gets the tag corresponding to the given property key if available.
        /// </summary>
        /// <typeparam name="T">The generic type of the property key.</typeparam>
        /// <param name="key">The property key for which to retrieve the value.</param>
        /// <returns>The value of the tag corresponding to the given property key if available, the default value of the property key otherwise.</returns>
        public T TryGetTag<T>(PropertyKey<T> key)
        {
            return Tags.Get(key);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <typeparam name="T"></typeparam>
        /// <param name="key"></param>
        /// <returns></returns>
        public bool HasTag<T>(PropertyKey<T> key)
        {
            return Tags.ContainsKey(key);
        }

        public void SetTag<T>(PropertyKey<T> key, T value)
        {
            Tags[key] = value;
        }

        protected virtual void ValidateParameters()
        {
            if (Name == null)
            {
                throw new InvalidOperationException($"[{nameof(Name)}] cannot be null in {GetType().Name}");
            }
            if (OutputDirectory == null && Description.Scope == TemplateScope.Session)
            {
                throw new InvalidOperationException($"[{nameof(OutputDirectory)}] cannot be null in {GetType().Name}");
            }
            if (Description == null)
            {
                throw new InvalidOperationException($"[{nameof(Description)}] cannot be null in {GetType().Name}");
            }
            if (Logger == null)
            {
                throw new InvalidOperationException($"[{nameof(Logger)}] cannot be null in {GetType().Name}");
            }
        }
    }
}
