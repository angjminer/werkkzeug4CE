/*+**************************************************************************/
/***                                                                      ***/
/***   This file is distributed under a BSD license.                      ***/
/***   See LICENSE.txt for details.                                       ***/
/***                                                                      ***/
/**************************************************************************+*/

#include "wz4_anim.hpp"
#include "wz4_anim_ops.hpp"
#include "wz4lib/serials.hpp"

/****************************************************************************/
/***                                                                      ***/
/***   Helpers                                                            ***/
/***                                                                      ***/
/****************************************************************************/

void Wz4AnimKey::Init()
{
  sClear(*this);
  Scale.Init(1,1,1);
}

void Wz4AnimKey::ToMatrix(sMatrix34 &mat) const
{
  mat.Init(Rot);
  mat.Scale(Scale.x,Scale.y,Scale.z);
  mat.l = Trans;
}

void Wz4AnimKey::ToMatrixInv(sMatrix34 &mat) const // this can be done more efficient!
{
  mat.Init(Rot);
  mat.Scale(Scale.x,Scale.y,Scale.z);
  mat.l = Trans;
  mat.InvertOrthogonal();
}

template <class streamer> void Wz4AnimKey::Serialize_(streamer &stream)
{
  sInt version = stream.Header(sSerId::Wz4AnimKey,1);
  if(version)
  {
    stream | Time | Weight | Scale | Rot | Trans | User;
    stream.Footer();
  }
}
void Wz4AnimKey::Serialize(sWriter &stream) { Serialize_(stream); }
void Wz4AnimKey::Serialize(sReader &stream) { Serialize_(stream); }

void Wz4AnimJoint::Init()
{
  Channel = 0;
  BasePose.Init();
  Parent = -1;
}

void SerializeWz4Channel(sReader &stream,Wz4Channel *&chan)
{
  switch(stream.PeekHeader())
  {
  case sSerId::Wz4ChannelPerFrame:
    { Wz4ChannelPerFrame *c = new Wz4ChannelPerFrame; c->Serialize(stream); chan=c; }
    break;
  case sSerId::Wz4ChannelSpline:
    { Wz4ChannelSpline *c = new Wz4ChannelSpline; c->Serialize(stream); chan = c; }
    break;
  default:
    sVERIFY(L"unknown channel format");
    break;
  }
}

void SerializeWz4Channel(sWriter &stream,Wz4Channel *chan)
{
  chan->Serialize(stream);
}

/****************************************************************************/
/***                                                                      ***/
/***   Various Channels                                                   ***/
/***                                                                      ***/
/****************************************************************************/

Wz4Channel::Wz4Channel()
{
  Type = Wz4ChannelType;
  Kind = Wz4ChannelKindIllegal;
  MaxTime = 1;
}

/****************************************************************************/

Wz4ChannelConstant::Wz4ChannelConstant()
{
  Start.Init();
  Kind = Wz4ChannelKindConstant;
}

void Wz4ChannelConstant::Evaluate(sF32 time,Wz4AnimKey &result)
{
  result = Start;
  result.Time = time;
}

Wz4Channel *Wz4ChannelConstant::CopyTo()
{
  Wz4ChannelConstant *ch = new Wz4ChannelConstant;
  ch->Start = Start;
  return ch;
}

/****************************************************************************/

Wz4ChannelLinear::Wz4ChannelLinear()
{
  Start.Init();
  Speed.Init(0,0,0);
  Gravity.Init(0,0,0);
  ScaleSpeed.Init(0,0,0);
  RotAxis.Init(0,1,0);
  RotSpeed = 0;
  UserStart.Init(0,0,0);
  UserSpeed.Init(0,0,0);
  Weight = 0;
  Kind = Wz4ChannelKindLinear;

  prepared = 0;
}

void Wz4ChannelLinear::Prepare()
{
  sMatrix34 mat = Start;
  mat.Trans3();
  ScaleStart.x = Start.i.Length();
  ScaleStart.y = Start.j.Length();
  ScaleStart.z = Start.k.Length();
  mat.i.Unit();
  mat.j.Unit();
  mat.k.Unit();
  mat.Trans3();
  RotStart.Init(mat);
}

void Wz4ChannelLinear::Evaluate(sF32 time,Wz4AnimKey &result)
{
  sF32 f = time/MaxTime;
  result.Time = time;
  result.Weight = Weight;
  result.pad = 0;
  result.Scale = ScaleStart + ScaleSpeed*f;
  if(RotSpeed)
  {
    sQuaternion a;
    a.Init(RotAxis,RotSpeed*f*sPI2F);
    result.Rot = RotStart * a;
  }
  else
  {
    result.Rot = RotStart;
  }
  result.Trans = Start.l + Speed*f + Gravity*f*f;
  result.User = UserStart + UserSpeed*f;
}

/****************************************************************************/

Wz4ChannelPerFrame::Wz4ChannelPerFrame()
{
  Start.Init();
  Kind = Wz4ChannelKindPerFrame;
  Keys = 0;
  Scale = 0;
  Rot = 0;
  Trans = 0;
  User = 0;
}

Wz4ChannelPerFrame::~Wz4ChannelPerFrame()
{
  delete[] Scale;
  delete[] Rot;
  delete[] Trans;
  delete[] User;
}

template <class streamer> void Wz4ChannelPerFrame::Serialize_(streamer &stream)
{
  sInt version = stream.Header(sSerId::Wz4ChannelPerFrame,1);
  if(version)
  {
    Start.Serialize(stream);
    stream | Keys;
    if(stream.IsWriting())
    {
      stream.If(Scale!=0);
      stream.If(Rot!=0);
      stream.If(Trans!=0);
      stream.If(User!=0);
    }
    else // reading
    {
      if(stream.If(0)) Scale = new sVector31[Keys];
      if(stream.If(0)) Rot   = new sQuaternion[Keys];
      if(stream.If(0)) Trans = new sVector31[Keys];
      if(stream.If(0)) User  = new sVector31[Keys];
    }

    if(Scale) stream.ArrayF32((sF32 *)Scale,Keys*3);
    if(Rot  ) stream.ArrayF32((sF32 *)Rot  ,Keys*4);
    if(Trans) stream.ArrayF32((sF32 *)Trans,Keys*3);
    if(User ) stream.ArrayF32((sF32 *)User ,Keys*3);

    stream.Footer();
  }
}
void Wz4ChannelPerFrame::Serialize(sWriter &stream) { Serialize_(stream); }
void Wz4ChannelPerFrame::Serialize(sReader &stream) { Serialize_(stream); }

Wz4Channel *Wz4ChannelPerFrame::CopyTo()
{
  Wz4ChannelPerFrame *dest = new Wz4ChannelPerFrame;
  dest->MaxTime = MaxTime;
  dest->Start = Start;
  dest->Keys = Keys;
  dest->Scale = 0;
  dest->Rot = 0;
  dest->Trans = 0;
  dest->User = 0;

  if(Scale)
  {
    dest->Scale = new sVector31[Keys];
    sCopyMem(dest->Scale,Scale,sizeof(sVector31)*Keys);
  }
  if(Rot)
  {
    dest->Rot   = new sQuaternion[Keys];
    sCopyMem(dest->Rot  ,Rot  ,sizeof(sQuaternion)*Keys);
  }
  if(Trans)
  {
    dest->Trans = new sVector31[Keys];
    sCopyMem(dest->Trans,Trans,sizeof(sVector31)*Keys);
  }
  if(User)
  {
    dest->User  = new sVector31[Keys];
    sCopyMem(dest->User ,User ,sizeof(sVector31)*Keys);
  }

  return dest;
}

void Wz4ChannelPerFrame::Evaluate(sF32 time,Wz4AnimKey &result)
{
  result = Start;
  if(Keys>1)
  {
    time = (time / MaxTime) * (Keys-1);
    sInt key = sClamp<sInt>(sInt(sRoundDown(time*1024)),0,Keys*1024-1025);
    sF32 f = (key&1023)/1024.0f;
    key = key/1024;

    if(Scale)
      result.Scale.Fade(f,Scale[key+0],Scale[key+1]);
    if(Rot)
      result.Rot.Fade(f,Rot[key+0],Rot[key+1]);
    if(Trans)
      result.Trans.Fade(f,Trans[key+0],Trans[key+1]);
  }
}

/****************************************************************************/

Wz4ChannelSpline::Wz4ChannelSpline()
{
  Start.Init();
  Kind = Wz4ChannelKindSpline;
}

Wz4ChannelSpline::~Wz4ChannelSpline()
{
}

template <class streamer> void Wz4ChannelSpline::Serialize_(streamer &stream)
{
  sInt version = stream.Header(sSerId::Wz4ChannelSpline,1);
  if(version)
  {
    stream | Keys;
    Start.Serialize(stream);
    Scale.Serialize(stream);
    Rot.Serialize(stream);
    Trans.Serialize(stream);

    stream.Footer();
  }
}

void Wz4ChannelSpline::Serialize(sWriter &stream) { Serialize_(stream); }
void Wz4ChannelSpline::Serialize(sReader &stream) { Serialize_(stream); }

sBool Wz4ChannelSpline::Approximate(const Wz4ChannelPerFrame& target,sF32 err)
{
  Start = target.Start;
  Keys = target.Keys;
  MaxTime = target.MaxTime;
  sBool ok = sTRUE;

  if(target.Scale)  ok &= Scale.FitToCurve((sVector30*) target.Scale,target.Keys,err,1.0f);
  if(target.Trans)  ok &= Trans.FitToCurve((sVector30*) target.Trans,target.Keys,err,1.0f);

  if(target.Rot)
  {
    // need to clean up rotation first (get rid of sign flips)
    sQuaternion* temp = new sQuaternion[target.Keys];
    sF32 sign = 1.0f;

    for(sInt i=0;i<target.Keys;i++)
    {
      if(i && dot(target.Rot[i-1],target.Rot[i]) < 0.0f)
        sign = -sign;

      temp[i] = sign * target.Rot[i];
    }

    sBool isOk = Rot.FitToCurve(temp,target.Keys,err,1.0f);
    ok &= isOk;
    delete[] temp;
  }

  if(Scale.IsConstant(err)) { Scale.Evaluate((sVector30&) Start.Scale,0.0f); Scale.Clear(); }
  if(Rot.IsConstant(err))   { Rot.Evaluate(Start.Rot,0.0f); Rot.Clear(); }
  if(Trans.IsConstant(err)) { Trans.Evaluate((sVector30&) Start.Trans,0.0f); Trans.Clear(); }

  return ok;
}

void Wz4ChannelSpline::Evaluate(sF32 time,Wz4AnimKey &result)
{
  result = Start;
  time /= MaxTime;

  if(!Scale.IsEmpty())  Scale.Evaluate((sVector30&) result.Scale,time);
  if(!Rot.IsEmpty())    Rot.Evaluate(result.Rot,time);
  if(!Trans.IsEmpty())  Trans.Evaluate((sVector30&) result.Trans,time);
}

/****************************************************************************/

Wz4ChannelCat::Wz4ChannelCat()
{
  Kind = Wz4ChannelKindCat;
}

Wz4ChannelCat::~Wz4ChannelCat()
{
  sReleaseAll(Channels);
}

void Wz4ChannelCat::Evaluate(sF32 time,Wz4AnimKey &result)
{
  Wz4AnimKey a,b;
  Wz4Channel *ch;
  sFORALL(Channels,ch)
  {
    if(_i==0)
    {
      ch->Evaluate(time,result);
    }
    else
    {
      ch->Evaluate(time,a);
      result.Trans = result.Trans + sVector30(a.Trans);
      result.Scale.x = result.Scale.x * a.Scale.x;
      result.Scale.y = result.Scale.y * a.Scale.y;
      result.Scale.z = result.Scale.z * a.Scale.z;
      result.Rot = result.Rot * a.Rot;
    }
  }
}

/****************************************************************************/
/***                                                                      ***/
/***   Skeleton                                                           ***/
/***                                                                      ***/
/****************************************************************************/

Wz4Skeleton::Wz4Skeleton()
{
  Type = Wz4SkeletonType;
  TotalTime = 1;
}

Wz4Skeleton::~Wz4Skeleton()
{
  Wz4AnimJoint *j;
  sFORALL(Joints,j)
    j->Channel->Release();
}

template <class streamer> void Wz4Skeleton::Serialize_(streamer &stream)
{
  Wz4AnimJoint *joint;
  sInt version = stream.Header(sSerId::Wz4Skeleton,2);
  if(version)
  {
    stream.Array(Joints);
    sFORALL(Joints,joint)
    {
      stream | joint->Name | joint->Parent | joint->BasePose;
      SerializeWz4Channel(stream,joint->Channel);
    }
    if(version>=2)
    {
      stream | TotalTime;
    }
    else
    {
      FixTime(1/60.0f);  // assuming 60 fps
    }
    stream.Footer();
  }
}
void Wz4Skeleton::Serialize(sWriter &stream) { Serialize_(stream); }
void Wz4Skeleton::Serialize(sReader &stream) { Serialize_(stream); }


void Wz4Skeleton::CopyFrom(Wz4Skeleton *src)
{
  Wz4AnimJoint *joint;
  Joints = src->Joints;
  TotalTime = src->TotalTime;
  sFORALL(Joints,joint)
    if(joint->Channel)
      joint->Channel = joint->Channel->CopyTo();
}

sInt Wz4Skeleton::FindJoint(const sChar *name)
{
  Wz4AnimJoint *joint;
  sFORALL(Joints,joint)
    if(sCmpString(name,joint->Name)==0)
      return _i;
  return -1;
}

void Wz4Skeleton::FixTime(sF32 spf)
{
  sInt max = 0;
  Wz4AnimJoint *joint;
  sFORALL(Joints,joint)
    if(joint->Channel->Kind==Wz4ChannelKindPerFrame)
      max = sMax(max,((Wz4ChannelPerFrame *)(joint->Channel))->Keys);
  TotalTime = max*spf;
}


void Wz4Skeleton::Evaluate(sF32 time,sMatrix34 *mata,sMatrix34 *basemat)
{
  Wz4AnimJoint *j;
  Wz4AnimKey key;
  sMatrix34 mat;

  if(0) // softimage hirachical scaling
  {
    sVector31 *scale = sALLOCSTACK(sVector31,Joints.GetCount());
    sVector31 trans;
    sMatrix34 *rotat = sALLOCSTACK(sMatrix34,Joints.GetCount());
    sMatrix34 *local = sALLOCSTACK(sMatrix34,Joints.GetCount());
    sFORALL(Joints,j)
    {
      sInt i = _i;
      if(j->Channel)
      {
        j->Channel->Evaluate(time,key);
      }
      else
      {
        key.Init();
      }
      if(j->Parent==-1)
      {
        scale[i] = key.Scale;
        trans = key.Trans;
        rotat[i].Init(key.Rot);
        key.ToMatrix(mat);
        local[i] = mat;
      }
      else
      {
        mat.Init(key.Rot);
        scale[i] = key.Scale * scale[j->Parent];
        rotat[i] = mat * rotat[j->Parent];
        trans = key.Trans * local[j->Parent];
        key.ToMatrix(mat);
        local[i] = mat * local[j->Parent];
      }

      sMatrix34 mat1,mat2,mat3;

      mat = rotat[i];
      mat.Scale(scale[i].x,scale[i].y,scale[i].z);
      mat.l = trans;
      mata[i] = mat;

      if(basemat)
        basemat[i] = j->BasePose * mata[i];
    }     
  }
  else
  {
    sFORALL(Joints,j)
    {
      if(j->Channel)
      {
        j->Channel->Evaluate(time,key);
        key.ToMatrix(mat);
      }
      else
      {
        mat.Init();
      }
      if(j->Parent==-1)
        mata[_i] = mat;
      else
        mata[_i] = mat * mata[j->Parent];
      if(basemat)
        basemat[_i] = j->BasePose * mata[_i];
    }
  }
}

void Wz4Skeleton::EvaluateCM(sF32 time,sMatrix34 *mata,sMatrix34CM *basemat)
{
  Wz4AnimJoint *j;
  Wz4AnimKey key;
  sMatrix34 mat;

  sFORALL(Joints,j)
  {
    if(j->Channel)
    {
      j->Channel->Evaluate(time,key);
      key.ToMatrix(mat);
    }
    else
    {
      mat.Init();
    }
    if(j->Parent==-1)
      mata[_i] = mat;
    else
      mata[_i] = mat * mata[j->Parent];
    basemat[_i] = j->BasePose * mata[_i];
  }
}

void Wz4Skeleton::EvaluateBlendCM(sF32 time1,sF32 time2,sF32 reftime,sMatrix34 *mata,sMatrix34CM *basemat)
{
  Wz4AnimJoint *j;
  Wz4AnimKey key;
  sMatrix34 mat,mat1,mat2,mat2r;

  sFORALL(Joints,j)
  {
    if(j->Channel)
    {
      j->Channel->Evaluate(time1,key);
      key.ToMatrix(mat1);
      j->Channel->Evaluate(time2,key);
      key.ToMatrix(mat2);
      j->Channel->Evaluate(reftime,key);
      key.ToMatrixInv(mat2r);
      mat = mat1*mat2r;
      mat = mat*mat2;
    }
    else
    {
      mat.Init();
    }
    if(j->Parent==-1)
      mata[_i] = mat;
    else
      mata[_i] = mat * mata[j->Parent];
    basemat[_i] = j->BasePose * mata[_i];
  }
}

void Wz4Skeleton::EvaluateFadeCM(sF32 time1,sF32 time2,sF32 fade,sMatrix34 *mata,sMatrix34CM *basemat)
{
  Wz4AnimJoint *j;
  Wz4AnimKey key;
  sMatrix34 mat,mat1,mat2;

  sFORALL(Joints,j)
  {
    if(j->Channel)
    {
      if(_i>=10 && _i<161)      // detuned fake: explodierende aug�pfel
      {
        j->Channel->Evaluate(time1,key);
        key.ToMatrix(mat);
      }
      else
      {
        j->Channel->Evaluate(time1,key);
        key.ToMatrix(mat1);
        j->Channel->Evaluate(time2,key);
        key.ToMatrix(mat2);
        mat.i.Fade(fade,mat1.i,mat2.i); mat.i.Unit();
        mat.j.Fade(fade,mat1.j,mat2.j); mat.j.Unit();
        mat.k.Fade(fade,mat1.k,mat2.k); mat.k.Unit();
  //      mat.k.Cross(mat.i,mat.j); mat.k.Unit();
  //      mat.i.Cross(mat.j,mat.k); mat.i.Unit();
        mat.l.Fade(fade,mat1.l,mat2.l);
      }
    }
    else
    {
      mat.Init();
    }
    if(j->Parent==-1)
      mata[_i] = mat;
    else
      mata[_i] = mat * mata[j->Parent];
    basemat[_i] = j->BasePose * mata[_i];
  }
}

/****************************************************************************/
// ASSIMP
/****************************************************************************/

#ifdef sCOMPIL_ASSIMP

sINLINE const sAiNodeAnim* Wz4Skeleton::WaiGetAnimNode(const std::string nodeName, sInt animSeq)
{
  const sAiAnimation * anim = WaiAnimations[animSeq];
  for (sU32 i=0; i<anim->mNumChannels; i++)
  {
    const sAiNodeAnim* pNodeAnim = anim->mChannels[i];
    if(strcmp(pNodeAnim->mNodeName.c_str(), nodeName.c_str()) == 0)
      return pNodeAnim;
  }

  return 0;
}

sINLINE void Wz4Skeleton::WaiEvaluateScaling(aiVector3D& out, sF32 time, const sAiNodeAnim* nodeAnim)
{
  if (nodeAnim->mNumScalingKeys == 1)
  {
    out = nodeAnim->mScalingKeys[0];
  }
  else
  {
    time *= (nodeAnim->mNumScalingKeys-1);
    sInt key = sClamp<sInt>(sInt(sRoundDown(time*1024)),0,nodeAnim->mNumScalingKeys*1024-1025);
    sF32 f = (key&1023)/1024.0f;
    key = key/1024;
    aiVector3D d = nodeAnim->mScalingKeys[key+1] - nodeAnim->mScalingKeys[key+0];
    out = nodeAnim->mScalingKeys[key+0] + f * d;
  }
}

sINLINE void Wz4Skeleton::WaiEvaluateRotation(aiQuaternion& out, sF32 time, const sAiNodeAnim* nodeAnim)
{  
  if (nodeAnim->mNumRotationKeys == 1)
  {
    out = nodeAnim->mRotationKeys[0];
  }
  else
  {
    time *= (nodeAnim->mNumRotationKeys-1);
    sInt key = sClamp<sInt>(sInt(sRoundDown(time*1024)),0,nodeAnim->mNumRotationKeys*1024-1025);
    sF32 f = (key&1023)/1024.0f;
    key = key/1024;
    aiQuaternion::Interpolate(out, nodeAnim->mRotationKeys[key+0], nodeAnim->mRotationKeys[key+1], f);
    out = out.Normalize();
  }
} 

sINLINE void Wz4Skeleton::WaiEvaluatePosition(aiVector3D& out, sF32 time, const sAiNodeAnim* nodeAnim)
{
  if (nodeAnim->mNumPositionKeys == 1)
  {
    out = nodeAnim->mPositionKeys[0];
  }
  else
  {
    time *= (nodeAnim->mNumPositionKeys-1);
    sInt key = sClamp<sInt>(sInt(sRoundDown(time*1024)),0,nodeAnim->mNumPositionKeys*1024-1025);
    sF32 f = (key&1023)/1024.0f;
    key = key/1024;
    aiVector3D d = nodeAnim->mPositionKeys[key+1] - nodeAnim->mPositionKeys[key+0];
    out = nodeAnim->mPositionKeys[key+0] + f * d;
  }
}

void Wz4Skeleton::WaiReadNodeTree(sF32 time, const sAiNode* node, const aiMatrix4x4 & parentTransform, sInt animSeq)
{
  aiMatrix4x4 nodeTransformation = node->mTransformation;
  const sAiNodeAnim * nodeAnim = WaiGetAnimNode(node->mName, animSeq);

  if (nodeAnim)
  {
    aiVector3D scale, translate;
    aiQuaternion rotate;

    WaiEvaluateScaling(scale, time, nodeAnim);
    WaiEvaluateRotation(rotate, time, nodeAnim);
    WaiEvaluatePosition(translate, time, nodeAnim);
    nodeTransformation = aiMatrix4x4(scale, rotate, translate);
  }

  aiMatrix4x4 globalTransformation = parentTransform * nodeTransformation;

  std::unordered_map<std::string, sU32>::iterator it = WaiBoneMap.find(node->mName);
  if(it != WaiBoneMap.end())
    Joints[it->second].FinalTransformation = WaiGlobalInverseTransform * globalTransformation * Joints[it->second].BoneOffset;

  for (sU32 i=0; i<node->mNumChildren; i++)
    WaiReadNodeTree(time, node->mChildren[i], globalTransformation, animSeq);
}

void Wz4Skeleton::WaiEvaluateAssimpCM(sF32 time,sMatrix34 *mata,sMatrix34CM *basemat, sInt animSeq)
{
  sVERIFY(WaiAnimations.GetCount());

  if(animSeq >= WaiAnimations.GetCount())
    animSeq = WaiAnimations.GetCount()-1;

  WaiReadNodeTree(time, WaiRootNode, WaiPreTransform, animSeq);

  Wz4AnimJoint * j;
  sFORALL(Joints, j)
  {
    basemat[_i].x.x = j->FinalTransformation.a1;
    basemat[_i].x.y = j->FinalTransformation.a2;
    basemat[_i].x.z = j->FinalTransformation.a3;
    basemat[_i].x.w = j->FinalTransformation.a4;

    basemat[_i].y.x = j->FinalTransformation.b1;
    basemat[_i].y.y = j->FinalTransformation.b2;
    basemat[_i].y.z = j->FinalTransformation.b3;
    basemat[_i].y.w = j->FinalTransformation.b4;

    basemat[_i].z.x = j->FinalTransformation.c1;
    basemat[_i].z.y = j->FinalTransformation.c2;
    basemat[_i].z.z = j->FinalTransformation.c3;
    basemat[_i].z.w = j->FinalTransformation.c4;
  }
}

#endif // sCOMPIL_ASSIMP
