// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.
#pragma once

#include "stdafx.h"
#include "SceneMapping.h"

using namespace System;
using namespace System::IO;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;
using namespace SiliconStudio::Core::Mathematics;

namespace SiliconStudio {
	namespace Xenko {
		namespace Importer {
			namespace FBX {

				ref class AnimationConverter
				{
				private:
					FbxScene* scene;
					bool exportedFromMaya;
					SceneMapping^ sceneMapping;

				public:
					AnimationConverter(SceneMapping^ sceneMapping)
					{
						if (sceneMapping == nullptr) throw gcnew ArgumentNullException("sceneMapping");

						this->sceneMapping = sceneMapping;
						this->scene = sceneMapping->Scene;;

						auto documentInfo = scene->GetDocumentInfo();
						if (documentInfo->Original_ApplicationName.Get() == "Maya")
							exportedFromMaya = true;
					}

					bool HasAnimationData()
					{
						int animStackCount = scene->GetMemberCount<FbxAnimStack>();

						if (animStackCount > 0)
						{
							bool check = true;
							for (int i = 0; i < animStackCount && check; ++i)
							{
								FbxAnimStack* animStack = scene->GetMember<FbxAnimStack>(i);
								int animLayerCount = animStack->GetMemberCount<FbxAnimLayer>();
								FbxAnimLayer* animLayer = animStack->GetMember<FbxAnimLayer>(0);

								check = check && CheckAnimationData(animLayer, scene->GetRootNode());
							}

							return check;
						}

						return false;
					}

					Dictionary<System::String^, AnimationClip^>^ ProcessAnimation()
					{
						auto animationClips = gcnew Dictionary<System::String^, AnimationClip^>();

						int animStackCount = scene->GetMemberCount<FbxAnimStack>();
						// We support only anim stack count.
						if (animStackCount > 1)
						{
							// TODO: Add a log
							animStackCount = 1;
						}

						for (int i = 0; i < animStackCount; ++i)
						{
							FbxAnimStack* animStack = scene->GetMember<FbxAnimStack>(i);
							int animLayerCount = animStack->GetMemberCount<FbxAnimLayer>();
							FbxAnimLayer* animLayer = animStack->GetMember<FbxAnimLayer>(0);

							// Optimized code
							// From http://www.the-area.com/forum/autodesk-fbx/fbx-sdk/resetpivotsetandconvertanimation-issue/page-1/
							scene->GetRootNode()->ResetPivotSet(FbxNode::eDestinationPivot);
							SetPivotStateRecursive(scene->GetRootNode());
							scene->GetRootNode()->ConvertPivotAnimationRecursive(animStack, FbxNode::eDestinationPivot, 30.0f);
							ProcessAnimationByCurve(animationClips, animLayer, scene->GetRootNode());
							scene->GetRootNode()->ResetPivotSet(FbxNode::eSourcePivot);

							// Reference code (Uncomment Optimized code to use this part)
							//scene->SetCurrentAnimationStack(animStack);
							//ProcessAnimation(animationClip, animStack, scene->GetRootNode());
						}

						return animationClips;
					}

					List<String^>^ ExtractAnimationNodesNoInit()
					{
						int animStackCount = scene->GetMemberCount<FbxAnimStack>();
						List<String^>^ animationNodes = nullptr;

						if (animStackCount > 0)
						{
							animationNodes = gcnew List<String^>();
							for (int i = 0; i < animStackCount; ++i)
							{
								FbxAnimStack* animStack = scene->GetMember<FbxAnimStack>(i);
								int animLayerCount = animStack->GetMemberCount<FbxAnimLayer>();
								FbxAnimLayer* animLayer = animStack->GetMember<FbxAnimLayer>(0);
								GetAnimationNodes(animLayer, scene->GetRootNode(), animationNodes);
							}
						}

						return animationNodes;
					}

				private:

					ref class CurveEvaluator
					{
						FbxAnimCurve* curve;
						int index;

					public:
						CurveEvaluator(FbxAnimCurve* curve)
							: curve(curve), index(0)
						{
						}

						float Evaluate(CompressedTimeSpan time)
						{
							auto fbxTime = FbxTime((long long)time.Ticks * FBXSDK_TIME_ONE_SECOND.Get() / (long long)CompressedTimeSpan::TicksPerSecond);
							int currentIndex = index;
							auto result = curve->Evaluate(fbxTime, &currentIndex);
							index = currentIndex;

							return result;
						}
					};

					template <class T>
					AnimationCurve<T>^ ProcessAnimationCurveVector(AnimationClip^ animationClip, String^ name, int numCurves, FbxAnimCurve** curves, float maxErrorThreshold)
					{
						auto keyFrames = ProcessAnimationCurveFloatsHelper<T>(curves, numCurves);
						if (keyFrames == nullptr)
							return nullptr;

						// Add curve
						auto animationCurve = gcnew AnimationCurve<T>();

						// Switch to cubic implicit interpolation mode for Vector3
						animationCurve->InterpolationType = AnimationCurveInterpolationType::Cubic;

						// Create keys
						for (int i = 0; i < keyFrames->Count; ++i)
						{
							animationCurve->KeyFrames->Add(keyFrames[i]);
						}

						animationClip->AddCurve(name, animationCurve);

						if (keyFrames->Count > 0)
						{
							auto curveDuration = keyFrames[keyFrames->Count - 1].Time;
							if (animationClip->Duration < curveDuration)
								animationClip->Duration = curveDuration;
						}

						return animationCurve;
					}

					template <class T> AnimationCurve<T>^ CreateCurve(AnimationClip^ animationClip, String^ name, List<KeyFrameData<T>>^ keyFrames)
					{
						// Add curve
						auto animationCurve = gcnew AnimationCurve<T>();

						if (T::typeid == Vector3::typeid)
						{
							// Switch to cubic implicit interpolation mode for Vector3
							animationCurve->InterpolationType = AnimationCurveInterpolationType::Cubic;
						}

						// Create keys
						for (int i = 0; i < keyFrames->Count; ++i)
						{
							animationCurve->KeyFrames->Add(keyFrames[i]);
						}

						animationClip->AddCurve(name, animationCurve);

						if (keyFrames->Count > 0)
						{
							auto curveDuration = keyFrames[keyFrames->Count - 1].Time;
							if (animationClip->Duration < curveDuration)
								animationClip->Duration = curveDuration;
						}

						return animationCurve;
					}

					AnimationCurve<Quaternion>^ ProcessAnimationCurveRotation(AnimationClip^ animationClip, String^ name, FbxAnimCurve** curves, float maxErrorThreshold, Matrix rotationAdjust)
					{
						auto keyFrames = ProcessAnimationCurveFloatsHelper<Vector3>(curves, 3);
						if (keyFrames == nullptr)
							return nullptr;

						// Convert euler angles to radians
						for (int i = 0; i < keyFrames->Count; ++i)
						{
							auto keyFrame = keyFrames[i];
							keyFrame.Value *= (float)Math::PI / 180.0f;
							keyFrames[i] = keyFrame;
						}

						// Add curve
						auto animationCurve = gcnew AnimationCurve<Quaternion>();

						// Create keys
						for (int i = 0; i < keyFrames->Count; ++i)
						{
							auto keyFrame = keyFrames[i];
							Vector3 rotation = keyFrame.Value;

							auto finalMatrix = Matrix::Transpose(rotationAdjust) * Matrix::RotationX(rotation.X) * Matrix::RotationY(rotation.Y) * Matrix::RotationZ(rotation.Z) * rotationAdjust;
							Vector3 scaling;
							Quaternion rotationQuat;
							Vector3 translation;
							finalMatrix.Decompose(scaling, rotationQuat, translation);

							KeyFrameData<Quaternion> newKeyFrame;
							newKeyFrame.Time = keyFrame.Time;
							newKeyFrame.Value = rotationQuat;
							animationCurve->KeyFrames->Add(newKeyFrame);
						}

						animationClip->AddCurve(name, animationCurve);

						if (keyFrames->Count > 0)
						{
							auto curveDuration = keyFrames[keyFrames->Count - 1].Time;
							if (animationClip->Duration < curveDuration)
								animationClip->Duration = curveDuration;
						}

						return animationCurve;
					}

					template <typename T>
					List<KeyFrameData<T>>^ ProcessAnimationCurveFloatsHelper(FbxAnimCurve** curves, int numCurves)
					{
						FbxTime startTime = FBXSDK_TIME_INFINITE;
						FbxTime endTime = FBXSDK_TIME_MINUS_INFINITE;
						for (int i = 0; i < numCurves; ++i)
						{
							auto curve = curves[i];
							if (curve == NULL)
								continue;

							FbxTimeSpan timeSpan;
							curve->GetTimeInterval(timeSpan);

							if (curve != NULL && curve->KeyGetCount() > 0)
							{
								auto firstKeyTime = curve->KeyGetTime(0);
								auto lastKeyTime = curve->KeyGetTime(curve->KeyGetCount() - 1);
								if (startTime > firstKeyTime)
									startTime = firstKeyTime;
								if (endTime < lastKeyTime)
									endTime = lastKeyTime;
							}
						}

						if (startTime == FBXSDK_TIME_INFINITE
							|| endTime == FBXSDK_TIME_MINUS_INFINITE)
						{
							// No animation
							return nullptr;
						}

						auto keyFrames = gcnew List<KeyFrameData<T>>();

						const float framerate = static_cast<float>(FbxTime::GetFrameRate(scene->GetGlobalSettings().GetTimeMode()));
						auto oneFrame = FbxTime::GetOneFrameValue(scene->GetGlobalSettings().GetTimeMode());

						// Step1: Pregenerate curve with discontinuities
						int currentKeyIndices[4];
						int currentEvaluationIndices[4];
						bool isConstant[4];

						for (int i = 0; i < numCurves; ++i)
						{
							// Start with current key at -1, so we properly advance to key 0 in the first iteration
							currentKeyIndices[i] = -1;
							currentEvaluationIndices[i] = 0;
							isConstant[i] = false;
						}

						//float values[4];
						auto key = KeyFrameData<T>();
						float* values = (float*)&key.Value;

						FbxTime time;
						bool lastFrame = false;
						for (time = startTime; time < endTime || !lastFrame; time += oneFrame)
						{
							// Last frame with time = endTime
							if (time >= endTime)
							{
								lastFrame = true;
								time = endTime;
							}

							key.Time = FBXTimeToTimeSpan(time);

							bool hasAnyDiscontinuity = false;
							bool hasDiscontinuity[4];

							for (int i = 0; i < numCurves; ++i)
							{
								auto curve = curves[i];
								if (curve != nullptr)
								{

									int currentIndex = currentKeyIndices[i];

									FbxAnimCurveKey curveKey;

									// Advance to appropriate key that should be active during this frame
									// (The current key is the latest key at or before the current time)
									bool wasConstant = false;
									while (currentIndex + 1 < curve->KeyGetCount() && curve->KeyGetTime(currentIndex + 1) <= time)
									{
										++currentIndex;

										// If we reached a new key and the previous one was constant, we have a discontinuity
										wasConstant = isConstant[i];

										auto interpolation = curve->KeyGetInterpolation(currentIndex);
										isConstant[i] = interpolation == FbxAnimCurveDef::eInterpolationConstant;
									}

									currentKeyIndices[i] = currentIndex;
									hasDiscontinuity[i] = wasConstant;
									hasAnyDiscontinuity |= wasConstant;

									// Update non-constant values
									if (!wasConstant)
									{
										values[i] = curve->Evaluate(time, &currentEvaluationIndices[i]);
									}
								}
								else
								{
									values[i] = 0;
								}
							}

							// If discontinuity, we need to add previous values twice (with updated time), and new values twice (with updated time) to ignore any implicit tangents
							if (hasAnyDiscontinuity)
							{
								keyFrames->Add(key);
								keyFrames->Add(key);
							}

							// Update constant values
							for (int i = 0; i < numCurves; ++i)
							{
								auto curve = curves[i];
								if (hasDiscontinuity[i])
									values[i] = curve == nullptr ? 0 : curve->Evaluate(time, &currentEvaluationIndices[i]);
							}

							keyFrames->Add(key);

							if (hasAnyDiscontinuity)
								keyFrames->Add(key);
						}

						return keyFrames;
					}

					void ConvertDegreeToRadians(AnimationCurve<float>^ channel)
					{
						for (int i = 0; i < channel->KeyFrames->Count; ++i)
						{
							auto keyFrame = channel->KeyFrames[i];
							keyFrame.Value *= (float)Math::PI / 180.0f;
							channel->KeyFrames[i] = keyFrame;
						}
					}

					void ReverseChannelZ(AnimationCurve<Vector3>^ channel)
					{
						// Used for handedness conversion
						for (int i = 0; i < channel->KeyFrames->Count; ++i)
						{
							auto keyFrame = channel->KeyFrames[i];
							keyFrame.Value.Z = -keyFrame.Value.Z;
							channel->KeyFrames[i] = keyFrame;
						}
					}

					void ComputeFovFromFL(AnimationCurve<float>^ channel, FbxCamera* pCamera)
					{
						// Used for handedness conversion
						for (int i = 0; i < channel->KeyFrames->Count; ++i)
						{
							auto keyFrame = channel->KeyFrames[i];
							keyFrame.Value = (float)(FocalLengthToVerticalFov(pCamera->FilmHeight.Get(), keyFrame.Value) * 180.0 / Math::PI);
							channel->KeyFrames[i] = keyFrame;
						}
					}

					void MultiplyChannel(AnimationCurve<float>^ channel, double factor)
					{
						// Used for handedness conversion
						for (int i = 0; i < channel->KeyFrames->Count; ++i)
						{
							auto keyFrame = channel->KeyFrames[i];
							keyFrame.Value = (float)(factor * keyFrame.Value);
							channel->KeyFrames[i] = keyFrame;
						}
					}

					void ProcessAnimationByCurve(Dictionary<String^, AnimationClip^>^ animationClips, FbxAnimLayer* animLayer, FbxNode* pNode)
					{
						auto animationClip = gcnew AnimationClip();

						auto nodeData = sceneMapping->FindNode(pNode);
						FbxAnimCurve* curves[3];

						auto nodeName = nodeData.Name;

						auto matrixModifier = sceneMapping->MatrixModifier;

						Vector3 translation;
						Matrix rotation;
						Vector3 scaling;
						matrixModifier.Decompose(scaling, rotation, translation);

						curves[0] = pNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X);
						curves[1] = pNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y);
						curves[2] = pNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z);
						auto translationCurve = ProcessAnimationCurveVector<Vector3>(animationClip, "Transform.Position", 3, curves, 0.005f);

						curves[0] = pNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X);
						curves[1] = pNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y);
						curves[2] = pNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z);
						ProcessAnimationCurveRotation(animationClip, "Transform.Rotation", curves, 0.01f, rotation);

						curves[0] = pNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X);
						curves[1] = pNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y);
						curves[2] = pNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z);
						auto scalingCurve = ProcessAnimationCurveVector<Vector3>(animationClip, "Transform.Scale", 3, curves, 0.005f);

						if (translationCurve != nullptr)
						{
							auto keyFrames = translationCurve->KeyFrames;
							for (int i = 0; i < keyFrames->Count; i++)
							{
								auto keyFrame = keyFrames[i];
								keyFrame.Value = (Vector3)Vector4::Transform(Vector4(keyFrame.Value, 1.0f), matrixModifier);
								keyFrames[i] = keyFrame;
							}
						}

						if (scalingCurve != nullptr)
						{
							auto keyFrames = scalingCurve->KeyFrames;
							for (int i = 0; i < keyFrames->Count; i++)
							{
								auto keyFrame = keyFrames[i];
								keyFrame.Value = Vector3::TransformNormal(keyFrame.Value, rotation);
								keyFrames[i] = keyFrame;
							}
						}

						FbxCamera* camera = pNode->GetCamera();
						if (camera != NULL)
						{
							if (camera->FieldOfViewY.GetCurve(animLayer))
							{
								curves[0] = camera->FieldOfViewY.GetCurve(animLayer);
								auto FovAnimChannel = ProcessAnimationCurveVector<float>(animationClip, "Camera.VerticalFieldOfView", 1, curves, 0.01f);

								// TODO: Check again Max
								//if (!exportedFromMaya)
								//	MultiplyChannel(FovAnimChannel, 0.6); // Random factor to match what we see in 3dsmax, need to check why!
							}


							if (camera->FocalLength.GetCurve(animLayer))
							{
								curves[0] = camera->FocalLength.GetCurve(animLayer);
								auto flAnimChannel = ProcessAnimationCurveVector<float>(animationClip, "Camera.VerticalFieldOfView", 1, curves, 0.01f);
								ComputeFovFromFL(flAnimChannel, camera);
							}

							if (camera->NearPlane.GetCurve(animLayer))
							{
								curves[0] = camera->NearPlane.GetCurve(animLayer);
								ProcessAnimationCurveVector<float>(animationClip, "Camera.NearClipPlane", 1, curves, 0.01f);
							}

							if (camera->FarPlane.GetCurve(animLayer))
							{
								curves[0] = camera->FarPlane.GetCurve(animLayer);
								ProcessAnimationCurveVector<float>(animationClip, "Camera.FarClipPlane", 1, curves, 0.01f);
							}
						}

						if (animationClip->Curves->Count > 0)
						{
							animationClips->Add(nodeName, animationClip);
						}

						for (int i = 0; i < pNode->GetChildCount(); ++i)
						{
							ProcessAnimationByCurve(animationClips, animLayer, pNode->GetChild(i));
						}
					}

					// This code is not used but is a reference code for code animation but less optimized than ProcessAnimationByCurve. 
					void ProcessAnimation(AnimationClip^ animationClip, FbxAnimStack* animStack, FbxNode* pNode)
					{
						auto layer0 = animStack->GetMember<FbxAnimLayer>(0);

						if (HasAnimation(layer0, pNode))
						{
							float start, end;
							const FbxTakeInfo* take_info = scene->GetTakeInfo(animStack->GetName());
							if (take_info)
							{
								start = (float)take_info->mLocalTimeSpan.GetStart().GetSecondDouble();
								end = (float)take_info->mLocalTimeSpan.GetStop().GetSecondDouble();
							}
							else
							{
								// Take the time line value.
								FbxTimeSpan lTimeLineTimeSpan;
								scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeLineTimeSpan);
								start = (float)lTimeLineTimeSpan.GetStart().GetSecondDouble();
								end = (float)lTimeLineTimeSpan.GetStop().GetSecondDouble();
							}

							auto evaluator = scene->GetAnimationEvaluator();

							auto animationName = animStack->GetName();

							// Create curves
							auto scalingFrames = gcnew List<KeyFrameData<Vector3>>();
							auto rotationFrames = gcnew List<KeyFrameData<Quaternion>>();
							auto translationFrames = gcnew List<KeyFrameData<Vector3>>();

							auto nodeData = sceneMapping->FindNode(pNode);

							auto parentNode = pNode->GetParent();
							auto nodeName = nodeData.Name;
							String^ parentNodeName = nullptr;
							if (parentNode != nullptr)
							{
								parentNodeName = sceneMapping->FindNode(parentNode).Name;
							}

							const float sampling_period = 1.f / 60.0f;
							bool loop_again = true;
							for (float t = start; loop_again; t += sampling_period) {
								if (t >= end) {
									t = end;
									loop_again = false;
								}

								auto fbxTime = FbxTimeSeconds(t);

								// Use GlobalTransform instead of LocalTransform
								auto fbxMatrix = evaluator->GetNodeGlobalTransform(pNode, fbxTime);
								if (parentNode != nullptr)
								{
									auto parentMatrixInverse = evaluator->GetNodeGlobalTransform(parentNode, fbxTime).Inverse();
									fbxMatrix = parentMatrixInverse * fbxMatrix;
								}
								auto matrix = sceneMapping->ConvertMatrixFromFbx(fbxMatrix);

								Vector3 scaling;
								Vector3 translation;
								Quaternion rotation;
								matrix.Decompose(scaling, rotation, translation);

								auto time = FBXTimeToTimeSpan(fbxTime);

								scalingFrames->Add(KeyFrameData<Vector3>(time, scaling));
								translationFrames->Add(KeyFrameData<Vector3>(time, translation));
								rotationFrames->Add(KeyFrameData<Quaternion>(time, rotation));
								//System::Diagnostics::Debug::WriteLine("[{0}] Parent:{1} Transform.Position[{2}] = {3}", t, parentNodeName, nodeName, translation);
								//System::Diagnostics::Debug::WriteLine("[{0}] Parent:{1} Transform.Rotation[{2}] = {3}", t, parentNodeName, nodeName, rotation);
								//System::Diagnostics::Debug::WriteLine("[{0}] Parent:{1} Transform.Scale[{2}] = {3}", t, parentNodeName, nodeName, scaling);
							}

							CreateCurve(animationClip, String::Format("Transform.Position[{0}]", nodeName), translationFrames);
							CreateCurve(animationClip, String::Format("Transform.Rotation[{0}]", nodeName), rotationFrames);
							CreateCurve(animationClip, String::Format("Transform.Scale[{0}]", nodeName), scalingFrames);
						}

						for (int i = 0; i < pNode->GetChildCount(); ++i)
						{
							ProcessAnimation(animationClip, animStack, pNode->GetChild(i));
						}
					}

					void SetPivotStateRecursive(FbxNode* pNode)
					{
						// From FbxNode.h
						FbxVector4 lZero(0, 0, 0);
						FbxVector4 lOne(1, 1, 1);
						pNode->SetPivotState(FbxNode::eSourcePivot, FbxNode::ePivotActive);
						pNode->SetPivotState(FbxNode::eDestinationPivot, FbxNode::ePivotActive);

						EFbxRotationOrder lRotationOrder;
						pNode->GetRotationOrder(FbxNode::eSourcePivot, lRotationOrder);
						pNode->SetRotationOrder(FbxNode::eDestinationPivot, lRotationOrder);

						//For cameras and lights (without targets) let's compensate the postrotation.
						if (pNode->GetCamera() || pNode->GetLight())
						{
							if (!pNode->GetTarget())
							{
								FbxVector4 lRV(90, 0, 0);
								if (pNode->GetCamera())
									lRV.Set(0, 90, 0);

								FbxVector4 prV = pNode->GetPostRotation(FbxNode::eSourcePivot);
								FbxAMatrix lSourceR;
								FbxAMatrix lR(lZero, lRV, lOne);
								FbxVector4 res = prV;

								// Rotation order don't affect post rotation, so just use the default XYZ order
								FbxRotationOrder rOrder;
								rOrder.V2M(lSourceR, res);

								lR = lSourceR * lR;
								rOrder.M2V(res, lR);
								prV = res;
								pNode->SetPostRotation(FbxNode::eSourcePivot, prV);
								pNode->SetRotationActive(true);
							}

							// Point light do not need to be adjusted (since they radiate in all the directions).
							if (pNode->GetLight() && pNode->GetLight()->LightType.Get() == FbxLight::ePoint)
							{
								pNode->SetPostRotation(FbxNode::eSourcePivot, FbxVector4(0, 0, 0, 0));
							}
						}
						// apply Pre rotations only on bones / end of chains
						if (pNode->GetNodeAttribute() && pNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton
							|| (pNode->GetMarker() && pNode->GetMarker()->GetType() == FbxMarker::eEffectorFK)
							|| (pNode->GetMarker() && pNode->GetMarker()->GetType() == FbxMarker::eEffectorIK))
						{
							if (pNode->GetRotationActive())
							{
								pNode->SetPreRotation(FbxNode::eDestinationPivot, pNode->GetPreRotation(FbxNode::eSourcePivot));
							}

							// No pivots on bones
							pNode->SetRotationPivot(FbxNode::eDestinationPivot, lZero);
							pNode->SetScalingPivot(FbxNode::eDestinationPivot, lZero);
							pNode->SetRotationOffset(FbxNode::eDestinationPivot, lZero);
							pNode->SetScalingOffset(FbxNode::eDestinationPivot, lZero);
						}
						else
						{
							// any other type: no pre-rotation support but...
							pNode->SetPreRotation(FbxNode::eDestinationPivot, lZero);

							// support for rotation and scaling pivots.
							pNode->SetRotationPivot(FbxNode::eDestinationPivot, pNode->GetRotationPivot(FbxNode::eSourcePivot));
							pNode->SetScalingPivot(FbxNode::eDestinationPivot, pNode->GetScalingPivot(FbxNode::eSourcePivot));
							// Rotation and scaling offset are supported
							pNode->SetRotationOffset(FbxNode::eDestinationPivot, pNode->GetRotationOffset(FbxNode::eSourcePivot));
							pNode->SetScalingOffset(FbxNode::eDestinationPivot, pNode->GetScalingOffset(FbxNode::eSourcePivot));
							//
							// If we don't "support" scaling pivots, we can simply do:
							// pNode->SetRotationPivot(FbxNode::eDestinationPivot, lZero);
							// pNode->SetScalingPivot(FbxNode::eDestinationPivot, lZero);
						}

						for (int i = 0; i < pNode->GetChildCount(); ++i)
						{
							SetPivotStateRecursive(pNode->GetChild(i));
						}
					}

					bool CheckAnimationData(FbxAnimLayer* animLayer, FbxNode* pNode)
					{
						if ((pNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X) != NULL
							&& pNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y) != NULL
							&& pNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z) != NULL)
							||
							(pNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X) != NULL
							&& pNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y) != NULL
							&& pNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z) != NULL)
							||
							(pNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X) != NULL
							&& pNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y) != NULL
							&& pNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z) != NULL))
							return true;

						FbxCamera* camera = pNode->GetCamera();
						if (camera != NULL)
						{
							if (camera->FieldOfViewY.GetCurve(animLayer))
								return true;

							if (camera->FocalLength.GetCurve(animLayer))
								return true;
						}

						for (int i = 0; i < pNode->GetChildCount(); ++i)
						{
							if (CheckAnimationData(animLayer, pNode->GetChild(i)))
								return true;
						}

						return false;
					}

					bool HasAnimation(FbxAnimLayer* animLayer, FbxNode* pNode)
					{
						return (pNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X) != NULL
							|| pNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y) != NULL
							|| pNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z) != NULL
							|| pNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X) != NULL
							|| pNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y) != NULL
							|| pNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z) != NULL
							|| pNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X) != NULL
							|| pNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y) != NULL
							|| pNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z) != NULL);
					}

					void GetAnimationNodes(FbxAnimLayer* animLayer, FbxNode* pNode, List<String^>^ animationNodes)
					{
						auto nodeData = sceneMapping->FindNode(pNode);;
						auto nodeName = nodeData.Name;

						bool checkTranslation = pNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X) != NULL;
						checkTranslation = checkTranslation || pNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y) != NULL;
						checkTranslation = checkTranslation || pNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z) != NULL;

						bool checkRotation = pNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X) != NULL;
						checkRotation = checkRotation || pNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y) != NULL;
						checkRotation = checkRotation || pNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z) != NULL;

						bool checkScale = pNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X) != NULL;
						checkScale = checkScale || pNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y) != NULL;
						checkScale = checkScale || pNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z) != NULL;

						if (checkTranslation || checkRotation || checkScale)
						{
							animationNodes->Add(nodeName);
						}
						else
						{
							bool checkCamera = true;
							FbxCamera* camera = pNode->GetCamera();
							if (camera != NULL)
							{
								if (camera->FieldOfViewY.GetCurve(animLayer))
									checkCamera = checkCamera && camera->FieldOfViewY.GetCurve(animLayer) != NULL;

								if (camera->FocalLength.GetCurve(animLayer))
									checkCamera = checkCamera && camera->FocalLength.GetCurve(animLayer) != NULL;

								if (checkCamera)
									animationNodes->Add(nodeName);
							}
						}

						for (int i = 0; i < pNode->GetChildCount(); ++i)
						{
							GetAnimationNodes(animLayer, pNode->GetChild(i), animationNodes);
						}
					}
				};

			}
		}
	}
}
