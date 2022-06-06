#include <cstdlib>
#include "GenerateAChrom.h"
#include "GenOperator.h"
#include "tools.hpp"

//{calculate the average execution time of tasks}
void W_Cal_Average(vector<double>& w) {
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        double sum = 0;
        int RscSize = Tasks[i].ElgRsc.size();
        for (int j = 0; j < RscSize; ++j)
            sum += 1.0 / Rscs[Tasks[i].ElgRsc[j]].pc;
        w[i] = Tasks[i].length * sum / RscSize;
    }
}

//{calculate the average transfer time among tasks}
void C_Cal_Average(vector<vector<double>>& c) {
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        if(Tasks[i].parents.size() == 0){
            continue;
        }
        for (int j = 0; j < Tasks[i].parents.size(); ++j) {
            int parent = Tasks[i].parents[j];
            double sum1 = 0;
            double sum = 0;
            sum = ParChildTranFileSizeSum[parent][i] / VALUE;
            for (int k = 0; k < Tasks[i].ElgRsc.size(); ++k) {
                for (int y = 0; y < Tasks[parent].ElgRsc.size(); ++y) {
                    if (Tasks[i].ElgRsc[k] == Tasks[parent].ElgRsc[y]) {
                        continue;
                    } else {
                        sum1 += sum * 8 / XY_MIN(Rscs[Tasks[i].ElgRsc[k]].bw, Rscs[Tasks[parent].ElgRsc[y]].bw);
                    }
                }
            }
            c[parent][i] = sum1 / (double) (Tasks[i].ElgRsc.size() * Tasks[parent].ElgRsc.size());
        }
    }
}

//calculate the rank of tasks based on independent IO using transfer time C[i][j]
void Calculate_Rank_b(vector<double>& RankList, vector<vector<double>>& c, vector<double>& w){
    for (int i = 0; i < TskLstInLvl[TskLstInLvl.size()-1].size(); ++i) {
        int TaskId=TskLstInLvl[TskLstInLvl.size()-1][i];
        RankList[TaskId] = w[TaskId];
    }
    for(int i =TskLstInLvl.size()-2 ;i >=0 ;--i){
        for (int j = 0; j < TskLstInLvl[i].size(); ++j) {
            int TaskId=TskLstInLvl[i][j];
            double ChildMaxRankc = 0;
            for (int k = 0; k < Tasks[TaskId].children.size(); ++k) {
                int tem = Tasks[TaskId].children[k];
                double CompareObject = RankList[tem] + c[TaskId][tem];
                if(ChildMaxRankc  < CompareObject ){
                    ChildMaxRankc = CompareObject;
                }
            }
            RankList[TaskId] = w[TaskId] + ChildMaxRankc;
        }
    }
}

void Calculate_Rank_t(vector<double>& RankList, vector<vector<double>>& c, vector<double>& w) {
    for(int i =1 ;i < TskLstInLvl.size(); ++i){
        for (int j = 0; j < TskLstInLvl[i].size(); ++j) {
            int TaskId = TskLstInLvl[i][j];
            for (int k = 0; k < Tasks[TaskId].parents.size(); ++k) {
                int tem = Tasks[TaskId].parents[k];
                double re = w[tem] + c[tem][TaskId] + RankList[tem];
                if (RankList[TaskId] < re) {
                    RankList[TaskId] = re;
                }
            }
        }
    }
}

//{initialize chromosome to allocate spaces}
void IntChr(chromosome& chrom) {
    chrom.TskSchLst.resize(comConst.NumOfTsk);
    chrom.RscAlcLst.resize(comConst.NumOfTsk);
    chrom.Code_RK.resize(comConst.NumOfTsk);
    chrom.RscAlcPart.resize(comConst.NumOfTsk);
    chrom.TskSchPart.resize(comConst.NumOfTsk);
    chrom.VTskSchPart.resize(comConst.NumOfTsk,0.0);
    chrom.VRscAlcPart.resize(comConst.NumOfTsk,0.0);
    chrom.EndTime.resize(comConst.NumOfTsk);
}

//{generate a topological sort randomly}
vector<int> GnrSS_TS() {
    vector<int> SS;
    vector<int> upr(comConst.NumOfTsk); //the variables for recording the numbers of unscheduled parent tasks
    vector<int> RTI;                    //the set for recording ready tasks whose parent tasks have been scheduled or not exist
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        upr[i] = Tasks[i].parents.size();
        if (upr[i]==0){
            RTI.push_back(i);
        }
    }
    while (!RTI.empty()) {
        int RandVec = rand() % RTI.size();
        int v = RTI[RandVec];
        vector<int>::iterator iter = RTI.begin() + RandVec;
        RTI.erase(iter);
        for (int i = 0; i < Tasks[v].children.size(); ++i) {
            --upr[Tasks[v].children[i]];
            if (upr[Tasks[v].children[i]] == 0) RTI.push_back(Tasks[v].children[i]);
        }
        SS.push_back(v);
    }
    return SS;
}

//{generate a task scheduling order by the levels of tasks from small to large} -xy2
//{Those haveing the same level are ranked arbitrarily among them} -xy2
vector<int> GnrSS_Lvl() {
    vector<int> ch;
    vector<vector<int>> tem = TskLstInLvl;
    for (int i = 0; i < TskLstInLvl.size(); ++i) {
        random_shuffle(tem[i].begin(), tem[i].end());   //arrange the tasks in each level
        for (int j = 0; j < tem[i].size(); ++j) {
            ch.push_back(tem[i][j]);
        }
    }
    return ch;
}

vector<double> GnrDecimalsByAscend() {
    vector<double> decimals(comConst.NumOfTsk);
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        decimals[i] = (i + RandomDouble(0,1)) / comConst.NumOfTsk;
    }
    return decimals;
}

// I/O independent
double DcdEvl(chromosome& ch, bool IsFrw) {
    double makespan = 0;
    vector<set<double> > ITL;                   //record the idle time-slot of all resources
    for (int j = 0; j < comConst.NumOfRsc; ++j) {
        set<double> a;
        a.insert(0.0); a.insert(InfiniteValue * 1.0);
        ITL.push_back(a);
    }
    //startDecode
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        int TaskIndex = ch.TskSchLst[i];
        int RscIndex = ch.RscAlcLst[TaskIndex];  //obtain the resource (Rsc) allocated to the task
        double ReadyTime = 0;
        if(IsFrw) {                              //forward-loading
            for (int j = 0; j < Tasks[TaskIndex].parents.size(); ++j) {
                int ParentTask = Tasks[TaskIndex].parents[j];
                int ParentRsc = ch.RscAlcLst[ParentTask];
                double fft = ch.EndTime[ParentTask];
                if(RscIndex != ParentRsc) {
                    fft += ParChildTranFileSizeSum[ParentTask][TaskIndex] / VALUE * 8 / (XY_MIN(Rscs[RscIndex].bw, Rscs[ParentRsc].bw)); // -xy
                }
                if (ReadyTime < fft) {
                    ReadyTime = fft;
                }
            }
        } else {                                //backward-loading
            for (int j = 0; j < Tasks[TaskIndex].children.size(); ++j) {
                int ChildTask = Tasks[TaskIndex].children[j];
                int ChildRsc = ch.RscAlcLst[ChildTask];
                double fft = ch.EndTime[ChildTask];
                if(RscIndex != ChildRsc) {
                    fft += ParChildTranFileSizeSum[TaskIndex][ChildTask] / VALUE * 8 / (XY_MIN(Rscs[RscIndex].bw, Rscs[ChildRsc].bw));
                }
                if (ReadyTime < fft) {
                    ReadyTime = fft;
                }
            }
        }
        double ExecutionTime = Tasks[TaskIndex].length / Rscs[RscIndex].pc;
        double StartTime = FindIdleTimeSlot(ITL[RscIndex],ExecutionTime,ReadyTime); //{find an idle time-slot in ITL which can finish the task  at the earliest}
        ch.EndTime[TaskIndex] = StartTime + ExecutionTime;
        if (makespan < ch.EndTime[TaskIndex]) {
            makespan = ch.EndTime[TaskIndex];
        }
        UpdateITL(ITL[RscIndex],StartTime,ch.EndTime[TaskIndex]);       //{update ITL}
    }
    ch.FitnessValue = makespan;
    return ch.FitnessValue;
}

double NrmDcd(chromosome& ch, bool IsFrw) {
    vector<int > upr(comConst.NumOfTsk,-1);
    list<int> RTI;
    if(IsFrw)
        for (int i = 0; i < comConst.NumOfTsk; ++i) {
            upr[i]=Tasks[i].parents.size();
            if (upr[i]==0)  RTI.push_back(i);
        }
    else
        for (int i = 0; i < comConst.NumOfTsk; ++i) {
            upr[i]=Tasks[i].children.size();
            if (upr[i]==0)  RTI.push_back(i);
        }
    //generate resource allocation list and task scheduling order list
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        ch.RscAlcLst[i] = floor(ch.Code_RK[i]);
        double tmp = 1;
        list<int>::iterator pit;
        for (list<int>::iterator lit = RTI.begin(); lit != RTI.end(); ++lit) {
            int decimal = ch.Code_RK[*lit] - floor(ch.Code_RK[*lit]);
            if (decimal < tmp) {
                tmp = decimal; pit = lit; //小数部分最小的那个优先调度
            }
        }
        ch.TskSchLst[i] = *pit;
        RTI.erase(pit);
        //更新RTI;
        if (IsFrw)
            for (int l = 0; l < Tasks[ch.TskSchLst[i]].children.size(); ++l) {
                int childId = Tasks[ch.TskSchLst[i]].children[l];
                upr[childId] = upr[childId] - 1;
                if (upr[childId]==0)   RTI.push_back(childId);
            }
        else
            for (int l = 0; l < Tasks[ch.TskSchLst[i]].parents.size(); ++l) {
                int parentId = Tasks[ch.TskSchLst[i]].parents[l];
                upr[parentId] = upr[parentId] - 1;
                if (upr[parentId]==0)  RTI.push_back(parentId);
            }
    }
    DcdEvl(ch, IsFrw);
    return ch.FitnessValue;
}

double HrsDcd_CTP(chromosome& ch) {
    vector<double> w(comConst.NumOfTsk, 0);
    vector<double> Rank_b(comConst.NumOfTsk, 0);
    vector<int> ind(comConst.NumOfTsk);
    vector<vector<double>> TransferTime(comConst.NumOfTsk, vector<double>(comConst.NumOfTsk,0));
    //{calculate the transfer time between tasks when resource(Rsc) allocation has been determined}
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        int RscIndex = floor(ch.Code_RK[i]);
        if(Tasks[i].parents.size() !=  0){
            for (int j = 0; j < Tasks[i].parents.size(); ++j) {
                int parent = Tasks[i].parents[j];
                int ParRsc = floor(ch.Code_RK[parent]);
                if(ParRsc != RscIndex){
                    TransferTime[parent][i] = ParChildTranFileSizeSum[parent][i] / VALUE * 8 / XY_MIN(Rscs[RscIndex].bw,Rscs[ParRsc].bw) ;
                }
            }
        }
        w[i] = Tasks[i].length / Rscs[RscIndex].pc;
    }
    Calculate_Rank_b(Rank_b,TransferTime, w);
    IndexSortByValueOnAscend(ind, Rank_b);
    vector<double> Decimals = GnrDecimalsByAscend();
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        int TaskIndex = ind[comConst.NumOfTsk - i - 1];
        ch.Code_RK[TaskIndex] = floor(ch.Code_RK[TaskIndex]) + Decimals[i];
    }
    NrmDcd(ch, true);
    return ch.FitnessValue;
}

double GnrMS_Evl(chromosome& ch) {
    for (int i = 0; i < comConst.NumOfTsk; ++i)
        ch.RscAlcLst[i] = -1;
    vector<set<double> > ITL;                           //the idle time-slot lists  for all resources
    double makespan = 0;
    for (int j = 0; j < comConst.NumOfRsc; ++j) {
        set<double> a;
        a.insert(0.0); a.insert(InfiniteValue * 1.0);
        ITL.push_back(a);
    }
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        int RscId = -1;
        int TaskId = ch.TskSchLst[i];
        double FinalEndTime = InfiniteValue;
        double FinalStartTime = 0;
        SeletRsc_EFT(ch,ITL,TaskId,RscId,FinalStartTime,FinalEndTime);
        ch.EndTime[TaskId] = FinalEndTime;
        ch.RscAlcLst[TaskId] = RscId;
        UpdateITL(ITL[RscId],FinalStartTime,FinalEndTime);     //{update ITL}
        makespan = XY_MAX(makespan, FinalEndTime);
    }
    ch.FitnessValue = makespan;
    return makespan;
}

double HrsDcd_EFT(chromosome& ch) {
    vector<set<double> > ITL;                           //the idle time-slot lists  for all resources
    double makespan = 0;
    for (int j = 0; j < comConst.NumOfRsc; ++j) {
        set<double> a;
        a.insert(0.0); a.insert(InfiniteValue * 1.0);
        ITL.push_back(a);
    }
    vector<int > upr(comConst.NumOfTsk,0.0);
    list<int> RTI;
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        upr[i]=Tasks[i].parents.size();
        if (upr[i]==0)  RTI.push_back(i);
    }

    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        int RscId = -1;
        double tmp = 1;
        list<int>::iterator pit;
        for (list<int>::iterator lit = RTI.begin(); lit != RTI.end(); ++lit) {
            double decimal = ch.Code_RK[*lit] - floor(ch.Code_RK[*lit]);
            if (decimal < tmp) {
                tmp = decimal; pit = lit; //小数部分最小的那个优先调度
            }
        }
        ch.TskSchLst[i] = *pit;
        RTI.erase(pit);
        double FinalEndTime = InfiniteValue;
        double FinalStartTime = 0;
        SeletRsc_EFT(ch,ITL,ch.TskSchLst[i],RscId,FinalStartTime,FinalEndTime);
        ch.EndTime[ch.TskSchLst[i]] = FinalEndTime;
        ch.Code_RK[ch.TskSchLst[i]] = RscId + tmp;
        ch.RscAlcLst[ch.TskSchLst[i]] = RscId;
        UpdateITL(ITL[RscId],FinalStartTime,FinalEndTime); //{update ITL}
        makespan = XY_MAX(makespan, FinalEndTime);
        for (int l = 0; l < Tasks[ch.TskSchLst[i]].children.size(); ++l) {
            int ChildId = Tasks[ch.TskSchLst[i]].children[l];
            upr[ChildId] = upr[ChildId] - 1;
            if (upr[ChildId]==0)   RTI.push_back(ChildId);
        }
    }
    ch.FitnessValue = makespan;
    return makespan;
}

chromosome GnrChr_HEFT_b(vector<double> Rank_b) {
    chromosome TemChrom;
    IntChr(TemChrom);
    IndexSortByValueOnDescend(TemChrom.TskSchLst, Rank_b);
    GnrMS_Evl(TemChrom);
    return TemChrom;
}

void SeletRsc_EFT(chromosome& ch, vector<set<double>>& ITL, int& TaskId, int& RscId, double& FinalStartTime, double& FinalEndTime) {
    for (int j = 0; j < Tasks[TaskId].ElgRsc.size(); ++j) {
        double ReadyTime = 0;
        int RscIdOfCrnTsk = Tasks[TaskId].ElgRsc[j];
        for (int n = 0; n < Tasks[TaskId].parents.size(); ++n) { //calculate the ready time of the task
            int PrnTskId = Tasks[TaskId].parents[n];
            int RscIdOfPrnTsk = ch.RscAlcLst[PrnTskId];
            double fft = ch.EndTime[PrnTskId];
            if(RscIdOfCrnTsk != RscIdOfPrnTsk){
                double TransferData = ParChildTranFileSizeSum[PrnTskId][TaskId];
                fft += TransferData / VALUE * 8 / (XY_MIN(Rscs[RscIdOfCrnTsk].bw,Rscs[RscIdOfPrnTsk].bw));
            }
            if (ReadyTime + PrecisionValue < fft){
                ReadyTime = fft;
            }
        }
        double ExeTime = Tasks[TaskId].length / Rscs[RscIdOfCrnTsk].pc;
        double StartTime = FindIdleTimeSlot(ITL[RscIdOfCrnTsk],ExeTime,ReadyTime); //Find an idle time-slot as early as possible from ITL
        double EndTime = StartTime + ExeTime;
        //{find/record the earliest finish time}
        if (EndTime + PrecisionValue < FinalEndTime) {
            FinalStartTime = StartTime;
            FinalEndTime = EndTime;
            RscId = RscIdOfCrnTsk;
        }
    }
}

double FindIdleTimeSlot(set<double>& ITLofRscId,double& ExeTime,double& ReadyTime){
    set<double>::iterator pre  = ITLofRscId.begin();
    set<double>::iterator post = ITLofRscId.begin();
    ++post;
    while(post != ITLofRscId.end()) {
        if((*post - *pre) > ExeTime - PrecisionValue && ReadyTime - PrecisionValue < (*post)-ExeTime) {
            return  XY_MAX(*pre, ReadyTime);
        } else {
            ++pre; ++pre; ++post; ++post;
        }
    }
}

void UpdateITL(set<double>& ITLofRscId,double& StartTime,double& EndTime){
    if(ITLofRscId.find(StartTime) != ITLofRscId.end()) {
        ITLofRscId.erase(StartTime);
    } else {
        ITLofRscId.insert(StartTime);
    }
    if(ITLofRscId.find(EndTime) != ITLofRscId.end()) {
        ITLofRscId.erase(EndTime);
    } else {
        ITLofRscId.insert(EndTime);
    }
}

// {in the SS, tasks are arranged according to the level form small to large, and the tasks in the same level are arranged in descend of the number of child tasks}
chromosome GnrChr_HEFT_Baseline() {
    chromosome TemChrom;
    IntChr(TemChrom);
    int ScheduleOrder = 0;
    for (int j = 0; j < TskLstInLvl.size(); ++j) {
        if (TskLstInLvl[j].size() < 2) {
            TemChrom.TskSchLst[ScheduleOrder++]=TskLstInLvl[j][0];
            continue;
        }
        vector<int> SonTaskNum;
        for (int i = 0; i < TskLstInLvl[j].size(); ++i)
            SonTaskNum.push_back(Tasks[TskLstInLvl[j][i]].children.size());

        vector<int> ind(TskLstInLvl[j].size());
        IndexSortByValueOnAscend(ind, SonTaskNum);
        for (int i = TskLstInLvl[j].size() - 1; i >= 0; i--) {
            TemChrom.TskSchLst[ScheduleOrder++] = TskLstInLvl[j][ind[i]];
        }
    }
    GnrMS_Evl(TemChrom);
    return TemChrom;
}

chromosome GnrPrtByRank_Rnd(vector<double>& Rank) {
    chromosome chrom;
    IntChr(chrom);
    for(int i = 0; i < comConst.NumOfTsk; ++i){
        chrom.RscAlcPart[i] =RandomDouble2(0,comConst.NumOfRsc-1); //RandomDouble2(0,comConst.NumOfRsc);//rand() % comConst.NumOfRsc + rand() % 1000 / 1000.0 - 0.5;
    }
    RepairMapAndGnrRscAlcLst(chrom); //GnrRscAlcLst(chrom); //
    chrom.TskSchPart = Rank;
    RepairPriorityAndGnrSchOrd(chrom);
    DcdEvl(chrom, true);
    return chrom;
}

chromosome GnrPrtByRank_EFT(vector<double>& Rank) {
    chromosome chrom;
    IntChr(chrom);
    chrom.TskSchPart = Rank;
    RepairPriorityAndGnrSchOrd(chrom);
    GnrMS_Evl(chrom);
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        chrom.RscAlcPart[i] = chrom.RscAlcLst[i] - 0.5 + (rand() % 10000) / 10000.0;
    }
    return chrom;
}

void RepairMapAndGnrRscAlcLst(chromosome& ch) {
    for(int i = 0; i < comConst.NumOfTsk; ++i){
        int RscId = round(ch.RscAlcPart[i]);
        if(RscId < Tasks[i].ElgRsc[0]) { //超出下限的处理
            ch.RscAlcPart[i] = Tasks[i].ElgRsc[0];
            ch.RscAlcLst[i] = Tasks[i].ElgRsc[0];
            continue;
        }
        if(RscId > Tasks[i].ElgRsc[Tasks[i].ElgRsc.size()-1]) { //超出上限的处理
            ch.RscAlcPart[i] = Tasks[i].ElgRsc[Tasks[i].ElgRsc.size()-1];
            ch.RscAlcLst[i] = Tasks[i].ElgRsc[Tasks[i].ElgRsc.size()-1];
            continue;
        }
        if(find(Tasks[i].ElgRsc.begin(), Tasks[i].ElgRsc.end(), RscId) == Tasks[i].ElgRsc.end()){ //不存在的处理
            if(Tasks[i].ElgRsc.size() == 1) {
                ch.RscAlcPart[i] = Tasks[i].ElgRsc[0];
                ch.RscAlcLst[i] = Tasks[i].ElgRsc[0];
            } else {
                int TemRscId = FindNearestRscId(i, ch.RscAlcPart[i]);
                ch.RscAlcPart[i] = TemRscId;
                ch.RscAlcLst[i] = TemRscId;
            }
            continue;
        }
        ch.RscAlcLst[i] = RscId;
    }
}

int FindNearestRscId(int TaskId, double value ) {
    for (int j = 0; j < Tasks[TaskId].ElgRsc.size()-1; ++j ){
        if (Tasks[TaskId].ElgRsc[j] < value && value < Tasks[TaskId].ElgRsc[j+1] ) {
            if ( Tasks[TaskId].ElgRsc[j+1] - value < value - Tasks[TaskId].ElgRsc[j] ) {
                return Tasks[TaskId].ElgRsc[j+1];
            } else {
                return Tasks[TaskId].ElgRsc[j];
            }
        }
    }
}

void RepairPriorityAndGnrSchOrd(chromosome& chrom) {
    vector<int> V, Q;
    vector<int> N(comConst.NumOfTsk, -1);
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        N[i] = round(chrom.TskSchPart[i]);
    }
    vector<int> upr(comConst.NumOfTsk, 0);
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        upr[i] = Tasks[i].parents.size();
        if (upr[i] == 0) {
            Q.push_back(i);
        }
    }
    int MaxV = -1;
    while (V.size() != comConst.NumOfTsk) {
        for (int i = 0; i < Q.size(); ++i) {
            int TaskId = Q[i];
            int MaxP = -1;
            for (int i1 = 0; i1 < Tasks[TaskId].parents.size(); ++i1) {
                if (MaxP < N[Tasks[TaskId].parents[i1]]) {
                    MaxP = N[Tasks[TaskId].parents[i1]];
                }
            }
            if (N[TaskId] <= MaxP) {
                N[TaskId] = MaxP + 1;
            }
            for (int i1 = 0; i1 < V.size(); ++i1) {
                if (N[TaskId] == N[V[i1]])  {
                    N[TaskId] = MaxV + 1;
                    MaxV += 1;
                    break;
                }
            }
            MaxV = XY_MAX(N[TaskId], MaxV);
            V.push_back(TaskId);
        }
        vector<int> TemQ;
        for (int i = 0; i < Q.size(); ++i) {
            int taskId = Q[i];
            for (int i2 = 0; i2 < Tasks[taskId].children.size(); ++i2) {
                int childId = Tasks[taskId].children[i2];
                upr[childId] = upr[childId] - 1;
                if (upr[childId] == 0) {
                    TemQ.push_back(childId);
                }
            }
        }
        Q = TemQ;
    }
//    chrom.TskSchPart = N;
    for (int i = 0; i < comConst.NumOfTsk; ++i) {
        chrom.TskSchPart[i] = N[i];
    }
    IndexSortByValueOnAscend(chrom.TskSchLst, N);
}

void UpdateParticle(chromosome &ch,chromosome &Pbest, chromosome &Gbest, double &runtime, double &SchTime){
    Parameter_HPSO.InertiaWeight = 0.1 * (1-(runtime / SchTime)) + 0.9;
    Parameter_HPSO.c1 = 2 * (1-(runtime / SchTime));
    Parameter_HPSO.c2 = 2 * (runtime / SchTime);
    double r1 = RandomDouble(0,1);
    double r2 = RandomDouble(0,1);
    for(int i = 0; i < comConst.NumOfTsk; ++i){
        ch.VTskSchPart[i] = Parameter_HPSO.InertiaWeight * ch.VTskSchPart[i] + Parameter_HPSO.c1 * r1 * (Pbest.TskSchPart[i] - ch.TskSchPart[i])
                               + Parameter_HPSO.c2 * r2 * (Gbest.TskSchPart[i] - ch.TskSchPart[i]);
        ch.TskSchPart[i] += ch.VTskSchPart[i];

        ch.VRscAlcPart[i] = Parameter_HPSO.InertiaWeight * ch.VRscAlcPart[i] + Parameter_HPSO.c1 * r1 * (Pbest.RscAlcPart[i] - ch.RscAlcPart[i])
                               + Parameter_HPSO.c2 * r2 * (Gbest.RscAlcPart[i] - ch.RscAlcPart[i]);
        ch.RscAlcPart[i] += ch.VRscAlcPart[i];
    }
    RepairMapAndGnrRscAlcLst(ch); //GnrRscAlcLst(ch); //
    RepairPriorityAndGnrSchOrd(ch);
}