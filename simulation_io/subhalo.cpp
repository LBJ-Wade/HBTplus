#include <iostream>
#include <new>
#include <algorithm>

#include "../datatypes.h"
#include "snapshot_number.h"
#include "subhalo.h"
HBTReal SubHalo_t::KineticDistance(const Halo_t &halo, const Snapshot_t& partsnap)
{
  HBTReal dx=partsnap.PeriodicDistance(halo.CenterOfMassComoving, ComovingPosition);
  HBTReal dv=distance(halo.AverageVelocityPhysical, PhysicalVelocity);
  HBTReal d=dv+partsnap.Header.Hz*partsnap.Header.time*dx;
  return (d>0?d:-d);
}

void MemberShipTable_t::Init(const HBTInt nhalos, const HBTInt nsubhalos, const float alloc_factor)
{
  RawLists.clear();
  RawLists.resize(nhalos+1);
  Lists.Bind(nhalos, RawLists.data()+1);

  AllMembers.clear();
  AllMembers.reserve(nsubhalos*alloc_factor); //allocate more for seed haloes.
  AllMembers.resize(nsubhalos);
}
void MemberShipTable_t::BindMemberLists()
{
  HBTInt offset=0;
  for(HBTInt i=0;i<RawLists.size();i++)
  {
	RawLists[i].Bind(RawLists[i].size(), &(AllMembers[offset]));
	offset+=RawLists[i].size();
	RawLists[i].Resize(0);
  }
}
void MemberShipTable_t::CountMembers(const SubHaloList_t& SubHalos)
{
  for(HBTInt subid=0;subid<SubHalos.size();subid++)
	Lists[SubHalos[subid].HostHaloId].IncrementSize();
}
void MemberShipTable_t::FillMemberLists(const SubHaloList_t& SubHalos)
{
  for(HBTInt subid=0;subid<SubHalos.size();subid++)
	Lists[SubHalos[subid].HostHaloId].push_back(subid);
}
struct CompareMass_t
{
  const SubHaloList_t * SubHalos;
  CompareMass_t(const SubHaloList_t & subhalos)
  {
	SubHalos=&subhalos;
  }
  bool operator () (const HBTInt & i, const HBTInt & j)
  {
	return (*SubHalos)[i].Nbound>(*SubHalos)[j].Nbound;
  }
};
void MemberShipTable_t::SortMemberLists(const SubHaloList_t & SubHalos)
{
  CompareMass_t compare_mass(SubHalos);
  for(HBTInt i=0;i<RawLists.size();i++)
	std::sort(RawLists[i].data(), RawLists[i].data()+RawLists[i].size(), compare_mass);
}
void MemberShipTable_t::Build(const HBTInt nhalos, const SubHaloList_t & SubHalos)
{
  Init(nhalos, SubHalos.size());
  CountMembers(SubHalos);
  BindMemberLists();
  FillMemberLists(SubHalos);
  SortMemberLists(SubHalos);
//   std::sort(AllMembers.begin(), AllMembers.end(), CompareHostAndMass);
}
/*
inline bool SubHaloSnapshot_t::CompareHostAndMass(const HBTInt& subid_a, const HBTInt& subid_b)
{//ascending in host id, descending in mass inside each host, and put NullHaloId to the beginning.
  SubHalo_t a=SubHalos[subid_a], b=SubHalos[subid_b];
  
  if(a.HostHaloId==b.HostHaloId) return (a.Nbound>b.Nbound);
  
  return (a.HostHaloId<b.HostHaloId); //(a.HostHaloId!=SpecialConst::NullHaloId)&&
}*/
void SubHaloSnapshot_t::AssignHost(const HaloSnapshot_t &halo_snap, const Snapshot_t &part_snap)
{
  vector<HBTInt> ParticleToHost(part_snap.GetNumberOfParticles(), SpecialConst::NullHaloId);
  for(int haloid=0;haloid<halo_snap.Halos.size();haloid++)
  {
	for(int i=0;i<halo_snap.Halos[haloid].Particles.size();i++)
	  ParticleToHost[i]=haloid;
  }
  for(HBTInt subid=0;subid<SubHalos.size();subid++)
  {
	//rely on track_particle
	SubHalos[subid].HostHaloId=ParticleToHost[SubHalos[subid].TrackParticleId];
	//alternatives: CoreTrack; Split;
  }
  //alternative: trim particles outside fof
    
  MemberTable.Build(halo_snap.Halos.size(), SubHalos);
}
#define CORE_SIZE_MIN 20
#define CORE_SIZE_FRAC 0.25
void SubHaloSnapshot_t::AverageCoordinates(const Snapshot_t& part_snap)
{
  for(HBTInt subid=0;subid<SubHalos.size();subid++)
  {
	ShallowList_t <HBTInt> core;
	int coresize=SubHalos[subid].Nbound*CORE_SIZE_FRAC;
	if(coresize<CORE_SIZE_MIN) coresize=CORE_SIZE_MIN;
	if(coresize>SubHalos[subid].Nbound) coresize=SubHalos[subid].Nbound;
	core.Bind(coresize, SubHalos[subid].Particles.data());
	
	part_snap.AveragePosition(SubHalos[subid].ComovingPosition, core);
	part_snap.AverageVelocity(SubHalos[subid].PhysicalVelocity, core);
  }
}

#define MAJOR_PROGENITOR_MASS_RATIO 0.67
void SubHaloSnapshot_t::DecideCentrals(const HaloSnapshot_t &halo_snap, const Snapshot_t & part_snap)
/* to select central subhalo according to KineticDistance, and move each central to the beginning of each list in MemberTable*/
{
  for(HBTInt hostid=0;hostid<halo_snap.Halos.size();hostid++)
  {
	MemberShipTable_t::MemberList_t &List=MemberTable.Lists[hostid];
	if(List.size()>1)
	{
	  int n_major=0;
	  HBTInt MassLimit=SubHalos[List[0]].Nbound*MAJOR_PROGENITOR_MASS_RATIO;
	  for(n_major=1;n_major<List.size();n_major++)
		if(SubHalos[List[n_major]].Nbound<MassLimit) break;
		if(n_major>1)
		{
		  HBTReal dmin=SubHalos[List[0]].KineticDistance(halo_snap.Halos[hostid], part_snap);
		  int icenter=0;
		  for(int i=1;i<n_major;i++)
		  {
			HBTReal d=SubHalos[List[i]].KineticDistance(halo_snap.Halos[hostid], part_snap);
			if(dmin>d)
			{
			  dmin=d;
			  icenter=i;
			}
		  }
		  if(icenter)  swap(List[0], List[icenter]);
		}
	}
  }
}

void SubHaloSnapshot_t::RefineParticles()
{//it's more expensive to build an exclusive list. so do inclusive here. 
  //TODO: ensure the inclusive unbinding is stable (contaminating particles from big subhaloes may hurdle the unbinding
  for(HBTInt subid=0;subid<SubHalos.size();subid++)
  {
	SubHalos[subid].unbind();
  }
}
