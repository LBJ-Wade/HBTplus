#ifndef SUBHALO_HEADER_INCLUDED
#define SUBHALO_HEADER_INCLUDED

#include <iostream>
#include <new>
#include <vector>

#include "../datatypes.h"
#include "snapshot_number.h"
#include "halo.h"

class TrackParticle_t
{
public:
  HBTInt TrackId;
  HBTInt TrackParticleId;
  HBTInt SnapshotIndexOfLastIsolation; //the last snapshot when it was a central, only considering past snapshots.
  HBTInt SnapshotIndexOfLastMaxMass; //the snapshot when it has the maximum subhalo mass, only considering past snapshots.
  HBTInt SnapshotIndexOfBirth;//when the subhalo first becomes resolved
  HBTInt SnapshotIndexOfDeath;//when the subhalo first becomes un-resolved.
  HBTInt LastMaxMass;
  HBTxyz ComovingPosition;
  HBTxyz PhysicalVelocity;
  TrackParticle_t()
  {
	TrackId=-1;
	TrackParticleId=HBTInt();
	SnapshotIndexOfLastIsolation=SpecialConst::NullSnapshotId;
	SnapshotIndexOfLastMaxMass=SpecialConst::NullSnapshotId;
	SnapshotIndexOfBirth=SpecialConst::NullSnapshotId;
	SnapshotIndexOfDeath=SpecialConst::NullSnapshotId;
  }
  void SetTrackParticle(const HBTInt particle_id)
  {
	TrackParticleId=particle_id;
  }
  void SetTrackId(const HBTInt track_id)
  {
	TrackId=track_id;
  }
};

class SubHalo_t: public TrackParticle_t
{
  typedef ShallowList_t <HBTInt> ParticleShallowList_t;
public:
  ParticleShallowList_t Particles;
  HBTInt Nbound, Nsrc;
  HBTInt HostHaloId;
  HBTReal RmaxComoving;
  HBTReal VmaxPhysical;
  HBTReal RPoissonComoving;
  SubHalo_t(): TrackParticle_t(), Particles(), Nbound(0), Nsrc(0)
  {
  }
  void unbind()
  {//TODO
  }
  HBTReal KineticDistance(const Halo_t & halo, const Snapshot_t & partsnap);
};

typedef DeepList_t <SubHalo_t> SubHaloList_t;

class MemberShipTable_t
/* list the subhaloes inside each host, rather than ordering the subhaloes 
 * 
 * the principle is to not move the objects, but construct a table of them, since moving objects will change their id (or index at least), introducing the trouble to re-index them and update the indexes in any existence references.
 */
{
public:
  typedef ShallowList_t<HBTInt> MemberList_t;  //list of members in a group
private:
  void Init(const HBTInt nhalos, const HBTInt nsubhalos, const float alloc_factor=1.2);
  void BindMemberLists();
  void FillMemberLists(const SubHaloList_t & SubHalos);
  void CountMembers(const SubHaloList_t & SubHalos);
  void SortMemberLists(const SubHaloList_t & SubHalos);
  vector <MemberList_t> RawLists; //list of subhaloes inside each host halo; contain one more group than halo catalogue, to hold field subhaloes.
  vector <HBTInt> AllMembers; //the storage for all the MemberList_t
public:
  ShallowList_t <MemberList_t> Lists; //offset to allow hostid=-1
  
  MemberShipTable_t(): RawLists(), AllMembers(), Lists()
  {
  }
  void Build(const HBTInt nhalos, const SubHaloList_t & SubHalos);
};
class SubHaloSnapshot_t: public SnapshotNumber_t
{  
  typedef DeepList_t <HBTInt> ParticleList_t;
public:
  
  ParticleList_t AllParticles; /*TODO: replace this with a list of vectors. each subhalo manages its memory individually! */
  SubHaloList_t SubHalos;
  MemberShipTable_t MemberTable;
  SubHaloSnapshot_t(): SnapshotNumber_t(), SubHalos(), AllParticles(), MemberTable()
  {
  }
  void Load(Parameter_t &param, int snapshot_index)
  {//TODO
	cout<<"SubHaloSnapshot_t::Load() not implemented yet\n";
	if(SnapshotIndex<HBTConfig.MinSnapshotIndex)
	{// LoadNull();
	}
	else
	{
	}
  }
  void Save(Parameter_t &param)
  {
	//TODO
	cout<<"Save() not implemted yet\n";
  }
  void Clear()
  {
	//TODO
	cout<<"Clean() not implemented yet\n";
  }
  void ParticleIdToIndex(Snapshot_t & snapshot)
  {
	for(HBTInt i=0;i<AllParticles.size();i++)
	  AllParticles[i]=snapshot.GetParticleIndex(AllParticles[i]);
  }
  void ParticleIndexToId(Snapshot_t & snapshot)
  {
	for(HBTInt i=0;i<AllParticles.size();i++)
	  AllParticles[i]=snapshot.GetParticleId(AllParticles[i]);
  }
  void AverageCoordinates(const Snapshot_t &part_snap);
  void AssignHost(const HaloSnapshot_t &halo_snap, const Snapshot_t &part_snap);
  void DecideCentrals(const HaloSnapshot_t &halo_snap, const Snapshot_t &part_snap);
  void RefineParticles();
};


#endif