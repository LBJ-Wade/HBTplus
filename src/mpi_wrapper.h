#ifndef HBT_MPI_WRAPPER_H
#define HBT_MPI_WRAPPER_H

#include <mpi.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <climits>

#include "datatypes.h"
#include "mymath.h"

class MpiWorker_t
{ 
public:
  int  NumberOfWorkers, WorkerId, NameLen;
  int NextWorkerId, PrevWorkerId;//for ring communication
  char HostName[MPI_MAX_PROCESSOR_NAME];
  MPI_Comm Communicator; //do not use reference
  MpiWorker_t(MPI_Comm comm): Communicator(comm) //the default initializer will copy a handle? doesn't matter.
  {
	MPI_Comm_size(comm,&NumberOfWorkers);
	MPI_Comm_rank(comm,&WorkerId);
	MPI_Get_processor_name(HostName, &NameLen);
	NextWorkerId=WorkerId+1;
	if(NextWorkerId==NumberOfWorkers) NextWorkerId=0;
	PrevWorkerId=WorkerId-1;
	if(PrevWorkerId<0) PrevWorkerId=NumberOfWorkers-1;
  }
  int size()
  {
	return NumberOfWorkers;
  }
  int rank()
  {
	return WorkerId;
  }
  int next()
  {
	return NextWorkerId;
  }
  int prev()
  {
	return PrevWorkerId;
  }
  int RankAdd(int diff)
  {
	return (WorkerId+diff)%NumberOfWorkers;
  }
  template <class T>
  void SyncContainer(T &x, MPI_Datatype dtype, int root_worker);
  template <class T>
  void SyncAtom(T &x, MPI_Datatype dtype, int root_worker);
  void SyncAtomBool(bool &x, int root);
  void SyncVectorBool(vector <bool>&x, int root);
};

template <class T>
void MpiWorker_t::SyncContainer(T &x, MPI_Datatype dtype, int root_worker)
{
  int len;
 
  if(root_worker==WorkerId)
  {
	len=x.size();
	if(len>=INT_MAX)
	throw runtime_error("Error: in SyncContainer(), sending more than INT_MAX elements with MPI causes overflow.\n");
  }
  MPI_Bcast(&len, 1, MPI_INT, root_worker, Communicator);
  
  if(root_worker!=WorkerId)
	x.resize(len);
  MPI_Bcast((void *)x.data(), len, dtype, root_worker, Communicator);
};
template <class T>
inline void MpiWorker_t::SyncAtom(T& x, MPI_Datatype dtype, int root_worker)
{
  MPI_Bcast(&x, 1, dtype, root_worker, Communicator);
}

template <class T>
void VectorAllToAll(MpiWorker_t &world, vector < vector<T> > &SendVecs, vector < vector <T> > &ReceiveVecs, MPI_Datatype dtype)
{
  vector <int> SendSizes(world.size()), ReceiveSizes(world.size());
  for(int i=0;i<world.size();i++)
	SendSizes[i]=SendVecs[i].size();
  MPI_Alltoall(SendSizes.data(), 1, MPI_INT, ReceiveSizes.data(), 1, MPI_INT, world.Communicator);
  
  ReceiveVecs.resize(world.size());
  for(int i=0;i<world.size();i++)
	ReceiveVecs[i].resize(ReceiveSizes[i]);
  
  vector <MPI_Datatype> SendTypes(world.size()), ReceiveTypes(world.size());
  for(int i=0;i<world.size();i++)
  {
	MPI_Aint p;
	MPI_Address(SendVecs[i].data(), &p);
	MPI_Type_create_hindexed(1, &SendSizes[i], &p, dtype, &SendTypes[i]);
	MPI_Type_commit(&SendTypes[i]);
	
	MPI_Address(ReceiveVecs[i].data(), &p);
	MPI_Type_create_hindexed(1, &ReceiveSizes[i], &p, dtype, &ReceiveTypes[i]);
	MPI_Type_commit(&ReceiveTypes[i]);
  }
  vector <int> Counts(world.size(),1), Disps(world.size(),0);
  MPI_Alltoallw(MPI_BOTTOM, Counts.data(), Disps.data(), SendTypes.data(),
				MPI_BOTTOM, Counts.data(), Disps.data(), ReceiveTypes.data(), world.Communicator);
  
  for(int i=0;i<world.size();i++)
  {
	MPI_Type_free(&SendTypes[i]);
	MPI_Type_free(&ReceiveTypes[i]);
  }
  
}

template <class Particle_T, class InParticleIterator_T, class OutParticleIterator_T>
void MyAllToAll(MpiWorker_t &world, vector <InParticleIterator_T> &InParticleIterator, vector <HBTInt> &InParticleCount, vector <OutParticleIterator_T> &OutParticleIterator, MPI_Datatype &MPI_Particle_T)
/*break the task into smaller pieces to avoid message size overflow*/
{
  //determine loops
	const int chunksize=1024*1024;
	HBTInt InParticleSum=accumulate(InParticleCount.begin(), InParticleCount.end(), (HBTInt)0);
	HBTInt Nloop=ceil(1.*InParticleSum/chunksize);
	MPI_Allreduce(MPI_IN_PLACE, &Nloop, 1, MPI_HBT_INT, MPI_MAX, world.Communicator);
	//prepare loop size
	vector <int> SendParticleCounts(world.size()), RecvParticleCounts(world.size()), SendParticleDisps(world.size()), RecvParticleDisps(world.size());
	vector <HBTInt> SendParticleRemainder(world.size());
	for(int rank=0;rank<world.size();rank++)
	{
	  SendParticleCounts[rank]=InParticleCount[rank]/Nloop+1;//distribute remainder to first few loops
	  SendParticleRemainder[rank]=InParticleCount[rank]%Nloop;
	}
	cout<<"transmitting.."<<Nloop<<" loops: ";
	//transmit
	vector <Particle_T> SendBuffer(chunksize+world.size()), RecvBuffer(chunksize+world.size());
	for(HBTInt iloop=0;iloop<Nloop;iloop++)
	{
	  cout<<iloop<<" ";
	  //header
	  for(int rank=0;rank<world.size();rank++)
	  {
		if(iloop==SendParticleRemainder[rank])//switch sendcount from n+1 to n
		  SendParticleCounts[rank]--;
	  }
	  MPI_Alltoall(SendParticleCounts.data(), 1, MPI_INT, RecvParticleCounts.data(), 1, MPI_INT, world.Communicator);
	  CompileOffsets(SendParticleCounts, SendParticleDisps);
	  CompileOffsets(RecvParticleCounts, RecvParticleDisps);
	  //pack
	  for(int rank=0;rank<world.size();rank++)
	  {
		auto buff_begin=SendBuffer.begin()+SendParticleDisps[rank];
		auto buff_end=buff_begin+SendParticleCounts[rank];
		auto &it_part=InParticleIterator[rank];
		for(auto it_buff=buff_begin;it_buff!=buff_end;++it_buff)
		{
		  *it_buff=move(*it_part);
		  ++it_part;
		}
	  }
	  //send
	  MPI_Alltoallv(SendBuffer.data(), SendParticleCounts.data(), SendParticleDisps.data(), MPI_Particle_T, RecvBuffer.data(), RecvParticleCounts.data(), RecvParticleDisps.data(), MPI_Particle_T, world.Communicator);
	  //unpack
	  for(int rank=0;rank<world.size();rank++)
	  {
		auto buff_begin=RecvBuffer.begin()+RecvParticleDisps[rank];
		auto buff_end=buff_begin+RecvParticleCounts[rank];
		auto &it_part=OutParticleIterator[rank];
		for(auto it_buff=buff_begin;it_buff!=buff_end;++it_buff)//unpack
		{
		  *it_part=move(*it_buff);
		  ++it_part;
		}
	  }
	}
}

#endif