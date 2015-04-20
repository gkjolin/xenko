﻿using System;
using System.IO;
using System.Text;
using SiliconStudio.Core.Serialization;
using SiliconStudio.Paradox.Graphics;
using SiliconStudio.Paradox.Shaders;
using SiliconStudio.Paradox.Shaders.Compiler;

namespace SiliconStudio.Paradox.Assets.Effect
{
    /// <summary>
    /// Use this class to generate a code with embedded effect bytecode.
    /// </summary>
    public static class EffectByteCodeToSourceCodeWriter
    {
        public static void Write(string name, CompilerParameters parameters, EffectBytecode effectData, TextWriter writer)
        {
            const string codeTemplate = @"//------------------------------------------------------------------------------
// <auto-generated>
//     Paradox Effect Compiler File Generated:
{0}//
//     Command Line: {7}
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

namespace {1} 
{{
    {2} class {3}
    {{
        {4} static readonly byte[] {5} = new byte[] {{
{6}
        }};
    }}
}}
";
            var effectToGenerateText = new StringBuilder();
            effectToGenerateText.AppendFormat("//     Effect [{0}]\r\n", name);

            var buffer = new MemoryStream();
            effectData.WriteTo(buffer);

            var bufferAsText = new StringBuilder();
            var bufferArray = buffer.ToArray();
            for (int i = 0; i < bufferArray.Length; i++)
            {
                bufferAsText.Append(bufferArray[i]).Append(", ");
                if (i > 0 && (i % 64) == 0)
                {
                    bufferAsText.AppendLine();
                }
            }

            var classDeclaration = parameters.Get(EffectSourceCodeKeys.ClassDeclaration);
            var fieldDeclaration = parameters.Get(EffectSourceCodeKeys.FieldDeclaration);
            var nameSpace = parameters.Get(EffectSourceCodeKeys.Namespace);
            var className = parameters.Get(EffectSourceCodeKeys.ClassName) ?? name;
            var fieldName = parameters.Get(EffectSourceCodeKeys.FieldName);

            var commandLine = string.Join(" ", Environment.GetCommandLineArgs());

            var graphicsPlatform = parameters.Get(CompilerParameters.GraphicsPlatformKey);
            string paradoxDefine = "undefined";
            switch (graphicsPlatform)
            {
                case GraphicsPlatform.Direct3D11:
                    paradoxDefine = "SILICONSTUDIO_PARADOX_GRAPHICS_API_DIRECT3D11";
                    break;
                case GraphicsPlatform.OpenGL:
                    paradoxDefine = "SILICONSTUDIO_PARADOX_GRAPHICS_API_OPENGLCORE";
                    break;
                case GraphicsPlatform.OpenGLES:
                    paradoxDefine = "SILICONSTUDIO_PARADOX_GRAPHICS_API_OPENGLES";
                    break;
            }

            writer.WriteLine("#if {0}", paradoxDefine);
            writer.Write(codeTemplate,
                         effectToGenerateText, // {0} 
                         nameSpace,            // {1} 
                         classDeclaration,     // {2} 
                         className,            // {3} 
                         fieldDeclaration,     // {4} 
                         fieldName,            // {5} 
                         bufferAsText,         // {6}
                         commandLine);         // {7}

            writer.WriteLine("#endif");

            writer.Flush();
        }
    }
}