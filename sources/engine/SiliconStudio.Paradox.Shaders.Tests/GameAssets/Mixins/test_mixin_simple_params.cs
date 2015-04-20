﻿// <auto-generated>
// Do not edit this file yourself!
//
// This code was generated by Paradox Shader Mixin Code Generator.
// To generate it yourself, please install SiliconStudio.Paradox.VisualStudio.Package .vsix
// and re-save the associated .pdxfx.
// </auto-generated>

using System;
using SiliconStudio.Core;
using SiliconStudio.Paradox.Effects;
using SiliconStudio.Paradox.Graphics;
using SiliconStudio.Paradox.Shaders;
using SiliconStudio.Core.Mathematics;
using Buffer = SiliconStudio.Paradox.Graphics.Buffer;

namespace Test7
{
    [DataContract]public partial class TestParameters : ShaderMixinParameters
    {
        public static readonly ParameterKey<bool> param1 = ParameterKeys.New<bool>();
        public static readonly ParameterKey<int> param2 = ParameterKeys.New<int>(1);
        public static readonly ParameterKey<string> param3 = ParameterKeys.New<string>("ok");
    };
    internal static partial class ShaderMixins
    {
        internal partial class DefaultSimpleParams  : IShaderMixinBuilder
        {
            public void Generate(ShaderMixinSource mixin, ShaderMixinContext context)
            {
                context.Mixin(mixin, "A");
                context.Mixin(mixin, "B");
                if (context.GetParam(TestParameters.param1))
                {
                    context.Mixin(mixin, "C");
                    mixin.AddMacro("param2", context.GetParam(TestParameters.param2));

                    {
                        var __mixinToCompose__ = "X";
                        var __subMixin = new ShaderMixinSource();
                        context.PushComposition(mixin, "x", __subMixin);
                        context.Mixin(__subMixin, __mixinToCompose__);
                        context.PopComposition();
                    }
                }
                else
                {
                    context.Mixin(mixin, "D");
                    mixin.AddMacro("Test", context.GetParam(TestParameters.param3));

                    {
                        var __mixinToCompose__ = "Y";
                        var __subMixin = new ShaderMixinSource();
                        context.PushComposition(mixin, "y", __subMixin);
                        context.Mixin(__subMixin, __mixinToCompose__);
                        context.PopComposition();
                    }
                }
            }

            [ModuleInitializer]
            internal static void __Initialize__()

            {
                ShaderMixinManager.Register("DefaultSimpleParams", new DefaultSimpleParams());
            }
        }
    }
}
