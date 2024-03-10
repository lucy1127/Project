#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>
#include <unordered_set>
#include <utility>
#include <windows.h>
#include <string>
#include <random>
#include <iterator> 
#include <cstdlib> 
#include <ctime>   


using json = nlohmann::json;

struct PartType
{
    int PartTypeId;
    double Height;
    double Length;
    double Width;
    double Area;
    double Volume;
};

struct OrderDetail
{
    PartType* partType;
    int Quantity;
    int Material;
    int Quality;
    int OrderId;
};

struct Order
{
    int OrderId;
    double DueDate;
    double ReleaseDate;
    double PenaltyCost;
    std::vector<OrderDetail> OrderList;
};

struct OrderInfo
{
    int OrderId;
    double DueDate;
    double ReleaseDate;
    double PenaltyCost;
};
struct Machine
{
    int MachineId;
    double Area, Height, Length, Width;
    std::vector<int> Materials;
    std::vector<std::vector<double>> MaterialSetup;
    std::vector<double> StartSetup;
    double ScanTime, RecoatTime, RemovalTime;
};

struct PartTypeOrderInfo
{
    int machineID;
    int batchId;
    int Material;
    PartType* partType;
    OrderInfo orderInfo;
};

struct Batch
{
    int batchId;
    int materialType;
    std::vector<PartTypeOrderInfo> parts;
    double totalArea;
};
struct DelayedBatchInfo
{
    int machineId;
    int BatchIndex;
    double WeightedDelay;
    double thisBatchTime;
    int newPosition;
};

struct MachineBatch
{
    int MachineId;
    double MachineArea;
    double RunningTime;
    double TotalWeightedDelay;
    std::vector<Batch> Batches;
    std::vector<DelayedBatchInfo> delayedBatchInfo;
};

struct DelayedBatch {
    Batch batch;
    double WeightedDelay;
    double time;
    int MachineId;
};

const Machine& findMachineById(const std::vector<std::pair<int, Machine>>& sortedMachines, int machineId) {
    auto it = std::find_if(sortedMachines.begin(), sortedMachines.end(),
        [machineId](const std::pair<int, Machine>& machinePair) {
            return machinePair.second.MachineId == machineId;
        });

    if (it != sortedMachines.end()) {
        return it->second;
    }
    else {
        throw std::runtime_error("Machine with ID " + std::to_string(machineId) + " not found.");
    }
}


bool compareMachines(const std::pair<int, Machine>& a, const std::pair<int, Machine>& b)
{
    double totalTimeA = a.second.ScanTime + a.second.RecoatTime;
    double totalTimeB = b.second.ScanTime + b.second.RecoatTime;
    return totalTimeA < totalTimeB;
}

std::vector<std::pair<int, Machine>> sortMachines(const std::map<int, Machine>& machines)
{
    std::vector<std::pair<int, Machine>> machinesVec(machines.begin(), machines.end());

    std::sort(machinesVec.begin(), machinesVec.end(), compareMachines);

    return machinesVec;
}

bool orderDetailComparator(const OrderDetail& a, const OrderDetail& b, const std::map<int, Order>& orders, const std::map<int, PartType>& partTypes)
{
    const Order& orderA = orders.at(a.OrderId);
    const Order& orderB = orders.at(b.OrderId);
    const PartType& partA = *a.partType;
    const PartType& partB = *b.partType;


    if (orderA.DueDate != orderB.DueDate)
    {
        return orderA.DueDate < orderB.DueDate;
    }

    if (orderA.PenaltyCost != orderB.PenaltyCost)
    {
        return orderA.PenaltyCost > orderB.PenaltyCost;
    }
    return partA.Area > partB.Area;
}

std::map<int, std::vector<OrderDetail>> sortMaterialClassifiedOrderDetails(
    const std::map<int, std::vector<OrderDetail>>& materialClassifiedOrderDetails,
    const std::map<int, Order>& orders,
    const std::map<int, PartType>& partTypes)
{
    std::map<int, std::vector<OrderDetail>> sortedMaterialOrderDetails;

    for (const auto& entry : materialClassifiedOrderDetails)
    {
        auto sortedOrderDetails = entry.second;

        std::sort(sortedOrderDetails.begin(), sortedOrderDetails.end(),
            [&](const OrderDetail& a, const OrderDetail& b)
            {
                return orderDetailComparator(a, b, orders, partTypes);
            });

        sortedMaterialOrderDetails[entry.first] = sortedOrderDetails;
    }

    return sortedMaterialOrderDetails;
}

std::map<int, std::vector<PartTypeOrderInfo>> generateFinalSortedPartTypes(
    const std::map<int, std::vector<OrderDetail>>& sortedMaterialOrderDetails,
    const std::map<int, Order>& orders,
    const std::map<int, PartType>& partTypes)
{
    std::map<int, std::vector<PartTypeOrderInfo>> finalInfoByMaterial;

    for (const auto& materialEntry : sortedMaterialOrderDetails)
    {
        std::vector<OrderDetail> allOrderDetails = materialEntry.second;

        std::sort(allOrderDetails.begin(), allOrderDetails.end(),
            [&](const OrderDetail& a, const OrderDetail& b)
            {
                return orderDetailComparator(a, b, orders, partTypes);
            });

        for (const auto& detail : allOrderDetails)
        {
            PartTypeOrderInfo info;
            info.partType = detail.partType;
            info.Material = detail.Material;
            const Order& order = orders.at(detail.OrderId);


            OrderInfo orderInfo;
            orderInfo.OrderId = order.OrderId;
            orderInfo.DueDate = order.DueDate;
            orderInfo.ReleaseDate = order.ReleaseDate;
            orderInfo.PenaltyCost = order.PenaltyCost;
            info.orderInfo = orderInfo;

            for (int i = 0; i < detail.Quantity; ++i)
            {
                finalInfoByMaterial[detail.Material].push_back(info);
            }
        }
    }

    return finalInfoByMaterial;
}

bool allMaterialsProcessed(const std::map<int, std::vector<PartTypeOrderInfo>>& sortedMaterials)
{
    for (const auto& materialEntry : sortedMaterials)
    {
        if (!materialEntry.second.empty())
        {

            return false;
        }
    }
    return true;
}

int selectMaterial(const std::map<int, std::vector<PartTypeOrderInfo>>& remainingMaterials)
{
    int selectedMaterial = -1;
    double earliestDueDate = std::numeric_limits<double>::max();
    double lowestPenaltyCost = std::numeric_limits<double>::max();
    double smallestVolume = std::numeric_limits<double>::max();

    for (const auto& materialEntry : remainingMaterials)
    {
        for (const auto& partInfo : materialEntry.second)
        {
            if (partInfo.orderInfo.DueDate < earliestDueDate ||
                (partInfo.orderInfo.DueDate == earliestDueDate && partInfo.orderInfo.PenaltyCost < lowestPenaltyCost) ||
                (partInfo.orderInfo.DueDate == earliestDueDate && partInfo.orderInfo.PenaltyCost == lowestPenaltyCost && partInfo.partType->Volume > smallestVolume))
            {

                earliestDueDate = partInfo.orderInfo.DueDate;
                lowestPenaltyCost = partInfo.orderInfo.PenaltyCost;
                smallestVolume = partInfo.partType->Volume;
                selectedMaterial = materialEntry.first;
            }
        }
    }

    return selectedMaterial;
}

int selectMachineWithLeastRunningTime(const std::vector<MachineBatch>& machineBatches)
{
    int selectedMachineId = -1;
    double leastRunningTime = std::numeric_limits<double>::max();

    for (const auto& machineBatch : machineBatches)
    {
        if (machineBatch.RunningTime < leastRunningTime)
        {
            leastRunningTime = machineBatch.RunningTime;
            selectedMachineId = machineBatch.MachineId;
        }
    }

    return selectedMachineId;
}

double calculateFinishTime(const Batch& batch, const int& machineId, const Machine* machine)
{
    double volumeTime = 0.0;
    double maxHeight = 0.0;


    for (const auto& partInfo : batch.parts)
    {
        volumeTime += partInfo.partType->Volume * machine->ScanTime;
        maxHeight = std::max(maxHeight, partInfo.partType->Height);
    }

    double heightTime = maxHeight * machine->RecoatTime;

    return std::max(volumeTime, heightTime);
}
bool canAddToBatchOrNeedNewBatch(const Batch& batch, int machineId, const Machine* machine)
{
    double volumeTime = 0.0;
    double maxHeight = 0.0;

    for (const auto& partInfo : batch.parts)
    {
        volumeTime += partInfo.partType->Volume * machine->ScanTime;
        maxHeight = std::max(maxHeight, partInfo.partType->Height);
    }

    double heightTime = maxHeight * machine->RecoatTime;
    return volumeTime < heightTime;
}
struct AllocationResult {
    Batch batch;
    std::map<int, std::vector<PartTypeOrderInfo>> updatedMaterials;
};
static int currentBatchId = 0;
AllocationResult allocateMaterialToMachine(MachineBatch& selectedMachineBatch, int selectedMaterial,
    std::map<int, std::vector<PartTypeOrderInfo>>& remainingMaterials,
    const Machine* machine) {
    Batch newBatch;
    newBatch.batchId = currentBatchId++;
    newBatch.materialType = selectedMaterial;
    newBatch.totalArea = 0.0;

    if (remainingMaterials.find(selectedMaterial) != remainingMaterials.end()) {
        auto& materialList = remainingMaterials[selectedMaterial];
        for (auto it = materialList.begin(); it != materialList.end();) {
            double partArea = it->partType->Area; // 确保partArea有一个合理的值

            if (newBatch.totalArea + partArea <= selectedMachineBatch.MachineArea) {
                newBatch.parts.push_back(*it);
                newBatch.totalArea += partArea; // 更新totalArea

                double finishTime = calculateFinishTime(newBatch, selectedMachineBatch.MachineId, machine);

                if (canAddToBatchOrNeedNewBatch(newBatch, selectedMachineBatch.MachineId, machine) &&
                    it->orderInfo.DueDate < finishTime) {
                    it = materialList.erase(it); // 成功添加到批次，从待处理材料中移除
                }
                else {
                    newBatch.parts.pop_back(); // 从批次中移除部件
                    newBatch.totalArea -= partArea; // 撤销totalArea的更新

                    if (newBatch.parts.empty()) {
                        break;
                    }
                    else {
                        ++it;
                    }
                }
            }
            else {
                ++it;
            }
        }

        // 对于空批次且材料列表不为空的情况，强制添加第一个部件
        if (newBatch.parts.empty() && !materialList.empty()) {
            double forcedPartArea = materialList.front().partType->Area; // 获取强制添加部件的面积
            newBatch.parts.push_back(materialList.front());
            newBatch.totalArea += forcedPartArea; // 确保更新totalArea以反映这个部件的面积
            materialList.erase(materialList.begin());
        }
    }

    return { newBatch, remainingMaterials };
}


void updateRemainingMaterials(std::map<int, std::vector<PartTypeOrderInfo>>& remainingMaterials, int selectedMaterialType)
{
    if (remainingMaterials.find(selectedMaterialType) != remainingMaterials.end())
    {
        if (remainingMaterials[selectedMaterialType].empty())
        {
            remainingMaterials.erase(selectedMaterialType);
        }
    }

}

int selectEarliestMaterial(const std::map<int, std::vector<PartTypeOrderInfo>>& sortedMaterials)
{
    int selectedMaterial = -1;
    double earliestDueDate = std::numeric_limits<double>::max();
    double highestPenaltyCost = -std::numeric_limits<double>::max();
    double largestArea = -std::numeric_limits<double>::max();

    for (const auto& materialEntry : sortedMaterials)
    {
        if (!materialEntry.second.empty())
        {
            const auto& orderInfo = materialEntry.second.front().orderInfo;
            const auto& partType = materialEntry.second.front().partType;
            if (orderInfo.DueDate < earliestDueDate ||
                (orderInfo.DueDate == earliestDueDate && orderInfo.PenaltyCost > highestPenaltyCost) ||
                (orderInfo.DueDate == earliestDueDate && orderInfo.PenaltyCost == highestPenaltyCost && partType->Area > largestArea))
            {
                earliestDueDate = orderInfo.DueDate;
                highestPenaltyCost = orderInfo.PenaltyCost;
                largestArea = partType->Area;
                selectedMaterial = materialEntry.first;
            }
        }
    }

    return selectedMaterial;
}


MachineBatch& findMachineBatchById(std::vector<MachineBatch>& machineBatches, int machineId)
{
    for (auto& machineBatch : machineBatches)
    {
        if (machineBatch.MachineId == machineId)
        {
            return machineBatch;
        }
    }

    throw std::runtime_error("MachineBatch with the specified ID not found.");
}


double calculateWeightedDelay(const Batch& batch, double runningTime)
{
    double weightedDelay = 0.0;

    for (const auto& partInfo : batch.parts)
    {
        double delayTime = runningTime - partInfo.orderInfo.DueDate;
        if (delayTime > 0)
        {
            weightedDelay += delayTime * partInfo.orderInfo.PenaltyCost;
        }
    }

    return weightedDelay;
}

std::vector<MachineBatch> createMachineBatches(
    const std::map<int, std::vector<PartTypeOrderInfo>>& sortedMaterials,
    const std::vector<std::pair<int, Machine>>& sortedMachines)
{
    std::vector<MachineBatch> machineBatches;
    machineBatches.reserve(sortedMachines.size());
    std::map<int, int> lastMaterialForMachine;

    for (const auto& machinePair : sortedMachines)
    {
        machineBatches.emplace_back(MachineBatch{
            machinePair.first,
            machinePair.second.Area,
            0.0, // RunningTime
            0.0  // TotalWeightedDelay
            });
    }

    auto remainingMaterials = sortedMaterials;

    while (!remainingMaterials.empty())
    {

        int selectedMaterial = selectEarliestMaterial(remainingMaterials);
        int selectedMachineId = selectMachineWithLeastRunningTime(machineBatches);
        MachineBatch& machineBatchRef = findMachineBatchById(machineBatches, selectedMachineId);

        auto machineIt = std::find_if(sortedMachines.begin(), sortedMachines.end(),
            [selectedMachineId](const std::pair<int, Machine>& element)
            { return element.first == selectedMachineId; });

        double setUpTime = 0.0;

        if (machineIt != sortedMachines.end()) {
            const Machine& machine = machineIt->second;

            if (lastMaterialForMachine.find(selectedMachineId) == lastMaterialForMachine.end() ||
                lastMaterialForMachine[selectedMachineId] != selectedMaterial) {

                setUpTime = (lastMaterialForMachine.find(selectedMachineId) == lastMaterialForMachine.end()) ?
                    machine.StartSetup[selectedMaterial] :
                    std::accumulate(machine.MaterialSetup[selectedMaterial].begin(),
                        machine.MaterialSetup[selectedMaterial].end(),
                        0.0);

                lastMaterialForMachine[selectedMachineId] = selectedMaterial;
            }

            machineBatchRef.RunningTime += setUpTime;
        }

        AllocationResult result = allocateMaterialToMachine(machineBatchRef, selectedMaterial, remainingMaterials, &machineIt->second);
        remainingMaterials = result.updatedMaterials;
        updateRemainingMaterials(remainingMaterials, selectedMaterial);

        double finishTime = calculateFinishTime(result.batch, machineBatchRef.MachineId, &machineIt->second);
        machineBatchRef.RunningTime += finishTime;

        machineBatchRef.Batches.push_back(result.batch);


        DelayedBatchInfo delayedInfo{
            machineBatchRef.MachineId,
            static_cast<int>(machineBatchRef.Batches.size()) - 1,
            calculateWeightedDelay(result.batch, machineBatchRef.RunningTime),
            machineBatchRef.RunningTime
        };

        if (delayedInfo.WeightedDelay > 0) {
            machineBatchRef.delayedBatchInfo.push_back(delayedInfo);
        }

        machineBatchRef.TotalWeightedDelay += delayedInfo.WeightedDelay;
        updateRemainingMaterials(remainingMaterials, selectedMaterial);
        lastMaterialForMachine[selectedMachineId] = selectedMaterial;

    }

    return machineBatches;
}


std::vector<DelayedBatch> extractDelayedBatches(std::vector<MachineBatch>& machineBatches) {
    std::vector<DelayedBatch> delayedBatches;

    for (auto& machineBatch : machineBatches) {

        for (int i = machineBatch.delayedBatchInfo.size() - 1; i >= 0; i--) {
            const auto& delayedInfo = machineBatch.delayedBatchInfo[i];
            if (delayedInfo.WeightedDelay > 0) {
                DelayedBatch delayedBatch;
                delayedBatch.MachineId = machineBatch.MachineId;
                delayedBatch.batch = machineBatch.Batches[delayedInfo.BatchIndex - 1];
                delayedBatch.time = machineBatch.delayedBatchInfo[delayedInfo.BatchIndex - 1].thisBatchTime;
                delayedBatch.WeightedDelay = machineBatch.delayedBatchInfo[delayedInfo.BatchIndex - 1].WeightedDelay;
                delayedBatches.push_back(delayedBatch);

                machineBatch.Batches.erase(machineBatch.Batches.begin() + delayedInfo.BatchIndex - 1);
            }
        }
    }
    return delayedBatches;
}


void updateMachineBatches(MachineBatch& machineBatch, const std::vector<std::pair<int, Machine>>& sortedMachines) {

    machineBatch.RunningTime = 0;
    machineBatch.TotalWeightedDelay = 0;

    const Machine* machine = nullptr;
    for (const auto& machinePair : sortedMachines)
    {
        if (machinePair.first == machineBatch.MachineId)
        {
            machine = &machinePair.second;
            break;
        }
    }

    machineBatch.delayedBatchInfo.clear();

    for (auto& batch : machineBatch.Batches) {
        double finishTime = calculateFinishTime(batch, machineBatch.MachineId, machine);
        machineBatch.RunningTime += finishTime;
        double batchDelay = calculateWeightedDelay(batch, machineBatch.RunningTime);
        machineBatch.TotalWeightedDelay += batchDelay;
        // std::cout << batchDelay << std::endl;
        // std::cout << machineBatch.TotalWeightedDelay << std::endl;
        batch.totalArea = 0;
        double totalArea = 0.0;
        for (const auto& partInfo : batch.parts) {
            if (partInfo.partType != nullptr) { // Safety check
                totalArea += partInfo.partType->Area;
            }
        }
        batch.totalArea = totalArea;

        if (batchDelay > 0) {
            DelayedBatchInfo dbInfo;
            dbInfo.BatchIndex = &batch - &machineBatch.Batches[0];
            dbInfo.WeightedDelay = batchDelay;
            dbInfo.thisBatchTime = machineBatch.RunningTime;
            machineBatch.delayedBatchInfo.push_back(dbInfo);
        }
    }


}


bool compareDelayedBatches(const DelayedBatch& a, const DelayedBatch& b) {
    const auto& partA = a.batch.parts.front().orderInfo;
    const auto& partB = b.batch.parts.front().orderInfo;

    const auto& AreaA = a.batch.parts.front().partType->Area;
    const auto& AreaB = b.batch.parts.front().partType->Area;

    if (partA.DueDate != partB.DueDate)
        return partA.DueDate < partB.DueDate;
    if (partA.PenaltyCost != partB.PenaltyCost)
        return partA.PenaltyCost > partB.PenaltyCost;
    return AreaA > AreaB;
}

double tryInsertBatch(MachineBatch& machineBatch, const Batch& batchToInsert, int position, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    MachineBatch tempMachineBatch = machineBatch;
    std::cout << "*************************" << std::endl;

    std::cout << tempMachineBatch.MachineId << std::endl;

    tempMachineBatch.Batches.insert(tempMachineBatch.Batches.begin() + position, batchToInsert);

    tempMachineBatch.RunningTime = 0.0;
    tempMachineBatch.TotalWeightedDelay = 0.0;

    const Machine* machine = nullptr;
    for (const auto& machinePair : sortedMachines) {
        if (machinePair.first == tempMachineBatch.MachineId) {
            machine = &machinePair.second;
            break;
        }
    }
    if (!machine) {
        return -1;
    }

    std::cout << machine->MachineId << std::endl;

    int lastMaterial = -1;
    for (size_t i = 0; i < tempMachineBatch.Batches.size(); ++i) {
        const Batch& currentBatch = tempMachineBatch.Batches[i];
        std::cout << tempMachineBatch.Batches.size() << std::endl;
        std::cout << currentBatch.batchId << std::endl;
        std::cout << currentBatch.totalArea << std::endl;
        std::cout << currentBatch.materialType << std::endl;

        if (i == 0 || lastMaterial != currentBatch.materialType) {
            if (i == 0) {
                tempMachineBatch.RunningTime += machine->StartSetup[currentBatch.materialType];
            }
            else {
                tempMachineBatch.RunningTime += std::accumulate(machine->MaterialSetup[currentBatch.materialType].begin(),
                    machine->MaterialSetup[currentBatch.materialType].end(), 0.0);
            }
        }

        double finishTime = calculateFinishTime(currentBatch, machine->MachineId, machine);
        tempMachineBatch.RunningTime += finishTime;

        double batchDelay = calculateWeightedDelay(currentBatch, tempMachineBatch.RunningTime);
        tempMachineBatch.TotalWeightedDelay += batchDelay;

        lastMaterial = currentBatch.materialType;
    }

    return tempMachineBatch.TotalWeightedDelay - machineBatch.TotalWeightedDelay; // 返回额外延迟
}


void insertBatch(MachineBatch* machineBatch, int position, DelayedBatch& delayedBatch, const std::vector<std::pair<int, Machine>>& sortedMachines) {

    machineBatch->Batches.insert(machineBatch->Batches.begin() + position, delayedBatch.batch);


    machineBatch->RunningTime = 0.0;
    machineBatch->TotalWeightedDelay = 0.0;
    machineBatch->delayedBatchInfo.clear();

    int lastMaterial = -1;
    for (size_t i = 0; i < machineBatch->Batches.size(); ++i) {
        const Batch& currentBatch = machineBatch->Batches[i];


        const Machine* machine = nullptr;
        for (const auto& machinePair : sortedMachines) {
            if (machinePair.first == machineBatch->MachineId) {
                machine = &machinePair.second;
                break;
            }
        }
        if (!machine) {
            continue; // 如果找不到机器，则跳过
        }

        // 如果材料类型改变，考虑设置时间
        if (i == 0 || lastMaterial != currentBatch.materialType) {
            // 添加材料更换的设置时间
            if (i == 0) {
                machineBatch->RunningTime += machine->StartSetup[currentBatch.materialType];
            }
            else {
                machineBatch->RunningTime += std::accumulate(machine->MaterialSetup[currentBatch.materialType].begin(),
                    machine->MaterialSetup[currentBatch.materialType].end(), 0.0);
            }
        }

        // 计算完成时间和延迟
        double finishTime = calculateFinishTime(currentBatch, machineBatch->MachineId, machine);
        machineBatch->RunningTime += finishTime;
        double batchDelay = calculateWeightedDelay(currentBatch, machineBatch->RunningTime);
        machineBatch->TotalWeightedDelay += batchDelay;

        // 如果存在延迟，则记录在 DelayedBatchInfo 中
        if (batchDelay > 0) {
            DelayedBatchInfo dbInfo;
            dbInfo.BatchIndex = i;
            dbInfo.WeightedDelay = batchDelay;
            dbInfo.thisBatchTime = machineBatch->RunningTime;
            dbInfo.newPosition = position; // 这个可能需要根据您的具体逻辑进行调整
            machineBatch->delayedBatchInfo.push_back(dbInfo);
        }

        lastMaterial = currentBatch.materialType; // 更新材料类型
    }
}


// 辅助函数：插入最后一个批次
void insertLastBatch(std::vector<MachineBatch>& machineBatches, DelayedBatch& lastBatch, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    double bestAdditionalDelay = std::numeric_limits<double>::max();
    MachineBatch* bestMachineBatch = nullptr;
    int bestPosition = -1;

    for (auto& machineBatch : machineBatches) {
        for (size_t i = 0; i <= machineBatch.Batches.size(); ++i) {
            double trialDelay = tryInsertBatch(machineBatch, lastBatch.batch, i, sortedMachines);
            double additionalDelay = trialDelay;
            if (additionalDelay < bestAdditionalDelay) {
                bestAdditionalDelay = additionalDelay;
                bestMachineBatch = &machineBatch;
                bestPosition = i;
            }
        }
    }

    if (bestMachineBatch && bestPosition >= 0) {
        insertBatch(bestMachineBatch, bestPosition, lastBatch, sortedMachines);
    }
}

void reintegrateDelayedBatches(std::vector<MachineBatch>& machineBatches, std::vector<DelayedBatch>& delayedBatches, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    auto it = delayedBatches.begin();
    while (it != delayedBatches.end()) {
        DelayedBatch& currentBatch = *it;

        double bestAdditionalDelay = std::numeric_limits<double>::max();
        MachineBatch* bestMachineBatch = nullptr;
        int bestPosition = -1;

        // 遍历机器批次以找到最佳插入点
        for (auto& machineBatch : machineBatches) {
            for (size_t i = 0; i <= machineBatch.Batches.size(); ++i) {
                double trialDelay = tryInsertBatch(machineBatch, currentBatch.batch, i, sortedMachines);
                double additionalDelay = trialDelay - machineBatch.TotalWeightedDelay;
                if (additionalDelay < bestAdditionalDelay) {
                    bestAdditionalDelay = additionalDelay;
                    bestMachineBatch = &machineBatch;
                    bestPosition = i;
                }
            }
        }

        // 决定插入批次
        if (bestMachineBatch && bestPosition >= 0) {
            insertBatch(bestMachineBatch, bestPosition, currentBatch, sortedMachines);
            it = delayedBatches.erase(it); // 删除已插入的批次并更新迭代器
        }
        else {
            ++it; // 如果未插入，则继续下一个
        }
    }
}

std::vector<DelayedBatch> extractAndRandomSelectDelayedBatches(std::vector<MachineBatch>& machineBatches, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    std::vector<DelayedBatch> allDelayedBatches;
    DelayedBatch maxDelayBatch;
    double maxDelay = -1;

    for (auto& machineBatch : machineBatches) {
        for (auto& dbInfo : machineBatch.delayedBatchInfo) {
            if (dbInfo.WeightedDelay > 0) {
                DelayedBatch delayedBatch;
                delayedBatch.batch = machineBatch.Batches[dbInfo.BatchIndex];
                delayedBatch.WeightedDelay = dbInfo.WeightedDelay;
                delayedBatch.time = dbInfo.thisBatchTime;
                delayedBatch.MachineId = machineBatch.MachineId;
                allDelayedBatches.push_back(delayedBatch);

                if (dbInfo.WeightedDelay > maxDelay) {
                    maxDelay = dbInfo.WeightedDelay;
                    maxDelayBatch = delayedBatch;
                }
            }
        }
    }

    // 2. 除了加权延迟最大的批次外，从剩余的延迟批次中随机选择其他批次
    std::vector<DelayedBatch> selectedBatches = { maxDelayBatch }; // 包括最大延迟批次
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(allDelayedBatches.begin(), allDelayedBatches.end(), g);

    std::uniform_int_distribution<> dist(1, allDelayedBatches.size() - 1); // 随机数量，至少选择一个
    size_t numBatchesToSelect = dist(g);
    for (size_t i = 0; i < numBatchesToSelect && i < allDelayedBatches.size(); ++i) {
        // 检查是否为最大延迟批次，通过比较某个唯一属性，如MachineId和WeightedDelay
        if (!(allDelayedBatches[i].MachineId == maxDelayBatch.MachineId &&
            allDelayedBatches[i].WeightedDelay == maxDelayBatch.WeightedDelay &&
            allDelayedBatches[i].time == maxDelayBatch.time)) {
            selectedBatches.push_back(allDelayedBatches[i]);
        }
    }
    // 3. 从MachineBatch中移除选中的延迟批次
    for (auto& selectedBatch : selectedBatches) {
        for (auto& machineBatch : machineBatches) {
            if (machineBatch.MachineId == selectedBatch.MachineId) {
                auto it = std::find_if(machineBatch.Batches.begin(), machineBatch.Batches.end(), [&](const Batch& b) {
                    return b.batchId == selectedBatch.batch.batchId;
                    });
                if (it != machineBatch.Batches.end()) {
                    machineBatch.Batches.erase(it);
                }
            }
        }
    }

    for (auto& machineBatch : machineBatches) {
        updateMachineBatches(machineBatch, sortedMachines);
    }
    return selectedBatches;
}

std::vector<PartTypeOrderInfo> extractAndRandomSelectParts(std::vector<MachineBatch>& machineBatches) {
    std::vector<PartTypeOrderInfo> allDelayedParts;
    DelayedBatch maxDelayBatch;
    double maxWeightedDelay = 0;
    int maxDelayBatchIndex = -1;
    for (auto& machineBatch : machineBatches) {
        for (auto& delayedInfo : machineBatch.delayedBatchInfo) {
            if (delayedInfo.WeightedDelay > 0) {
                if (delayedInfo.BatchIndex <= 0 || delayedInfo.BatchIndex > machineBatch.Batches.size()) {
                    continue;
                }
                Batch& delayedBatch = machineBatch.Batches[delayedInfo.BatchIndex - 1];
                for (auto& part : delayedBatch.parts) {
                    part.machineID = machineBatch.MachineId;
                    part.batchId = delayedBatch.batchId;
                    allDelayedParts.push_back(part);
                }
                if (delayedInfo.WeightedDelay > maxWeightedDelay) {
                    maxWeightedDelay = delayedInfo.WeightedDelay;
                    maxDelayBatch = { delayedBatch, delayedInfo.WeightedDelay };
                    maxDelayBatchIndex = allDelayedParts.size() - delayedBatch.parts.size(); // 记录最大延迟批次的起始索引
                }
            }
        }
    }

    if (allDelayedParts.empty()) {
        return {};
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::vector<PartTypeOrderInfo> maxDelayParts;
    if (maxDelayBatchIndex != -1) {
        if (maxDelayBatchIndex < 0 || maxDelayBatchIndex >= allDelayedParts.size()) {
            // 最大延迟批次的起始索引超出范围，直接返回
            return {};
        }
        auto maxDelayStart = allDelayedParts.begin() + maxDelayBatchIndex;
        auto maxDelayEnd = maxDelayStart + maxDelayBatch.batch.parts.size();
        if (maxDelayEnd > allDelayedParts.end()) {
            return {};
        }
        maxDelayParts.assign(maxDelayStart, maxDelayEnd);
        allDelayedParts.erase(maxDelayStart, maxDelayEnd);
    }

    std::shuffle(allDelayedParts.begin(), allDelayedParts.end(), g);
    std::uniform_int_distribution<int> dist(0, allDelayedParts.size() - 1);
    int beta = dist(g) + maxDelayParts.size();
    std::vector<PartTypeOrderInfo> selectedParts = maxDelayParts;
    for (int i = 0; i < beta - maxDelayParts.size() && i < allDelayedParts.size(); ++i) {
        selectedParts.push_back(allDelayedParts[i]);
    }

    return selectedParts;
}


void updateMachineBatchesAfterExtraction(std::vector<MachineBatch>& machineBatches, const std::vector<PartTypeOrderInfo>& extractedParts, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    // 遍歷所有機器批次
    for (auto& machineBatch : machineBatches) {
        // 使用迭代器遍歷批次，以便可以在迭代過程中刪除元素
        for (auto batchIt = machineBatch.Batches.begin(); batchIt != machineBatch.Batches.end();) {
            bool batchIsEmpty = true; // 假設該批次最初是空的

            // 在每個批次中查找並刪除匹配的零件
            for (auto partIt = batchIt->parts.begin(); partIt != batchIt->parts.end();) {
                // 查找當前零件是否在抽取的零件列表中
                auto foundIt = std::find_if(extractedParts.begin(), extractedParts.end(), [&](const PartTypeOrderInfo& extractedPart) {
                    return extractedPart.batchId == batchIt->batchId &&
                        extractedPart.Material == partIt->Material &&
                        extractedPart.orderInfo.OrderId == partIt->orderInfo.OrderId &&
                        extractedPart.partType->PartTypeId == partIt->partType->PartTypeId &&
                        extractedPart.partType->Area == partIt->partType->Area;
                    });

                if (foundIt != extractedParts.end()) {
                    // 如果找到，刪除零件並更新 batchIsEmpty 標記
                    partIt = batchIt->parts.erase(partIt);
                }
                else {
                    ++partIt;
                    batchIsEmpty = false; // 批次中至少有一個零件，所以它不是空的
                }
            }

            // 檢查批次是否為空，如果是，則從機器批次中移除該批次
            if (batchIsEmpty) {
                batchIt = machineBatch.Batches.erase(batchIt);
            }
            else {
                ++batchIt;
            }
        }

        // 在刪除零件後更新機器批次
        updateMachineBatches(machineBatch, sortedMachines);
    }
}


bool partComparator(const PartTypeOrderInfo& a, const PartTypeOrderInfo& b) {
    if (a.orderInfo.DueDate != b.orderInfo.DueDate)
        return a.orderInfo.DueDate < b.orderInfo.DueDate;
    if (a.orderInfo.PenaltyCost != b.orderInfo.PenaltyCost)
        return a.orderInfo.PenaltyCost > b.orderInfo.PenaltyCost;

    if (a.partType != nullptr && b.partType != nullptr) {
        if (a.partType->Volume != b.partType->Volume)
            return a.partType->Volume > b.partType->Volume;
    }
    else {
        if (a.partType == nullptr)
            return false;
        if (b.partType == nullptr)
            return true;
    }

    return false;
}

bool canInsertPartToBatch(const PartTypeOrderInfo& partInfo, const Batch& batch, const Machine& machine) {
    if (partInfo.Material != batch.materialType) {
        return false;
    }
    double newTotalArea = batch.totalArea + partInfo.partType->Area;
    if (newTotalArea > machine.Area) {
        return false;
    }

    return true;
}

std::tuple<int, int> findOverallBestInsertionPosition(std::vector<MachineBatch>& machineBatches, const PartTypeOrderInfo& partInfo, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    int bestMachineIndex = -1;
    int bestBatchIndex = -1;
    double bestAdditionalDelay = std::numeric_limits<double>::max();

    for (int machineIndex = 0; machineIndex < machineBatches.size(); ++machineIndex) {
        MachineBatch& machineBatch = machineBatches[machineIndex];
        const Machine& machine = findMachineById(sortedMachines, machineBatch.MachineId);

        for (int batchIndex = 0; batchIndex < machineBatch.Batches.size(); ++batchIndex) {
            Batch& batch = machineBatch.Batches[batchIndex];

            if (canInsertPartToBatch(partInfo, batch, machine)) {
                Batch tempBatch = batch;
                tempBatch.parts.push_back(partInfo);
                double trialDelay = calculateWeightedDelay(tempBatch, machineBatch.RunningTime);
                double additionalDelay = trialDelay - machineBatch.TotalWeightedDelay;

                if (additionalDelay < bestAdditionalDelay) {
                    bestAdditionalDelay = additionalDelay;
                    bestMachineIndex = machineIndex;
                    bestBatchIndex = batchIndex;
                }
            }
        }
    }

    return std::make_tuple(bestMachineIndex, bestBatchIndex);
}
std::tuple<int, int> findBestInsertionPosition(std::vector<MachineBatch>& machineBatches, const PartTypeOrderInfo& partInfo, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    int bestMachineIndex = -1;
    int bestBatchIndex = -1;
    double bestAdditionalDelay = std::numeric_limits<double>::max();
    double leastRunningTime = std::numeric_limits<double>::max();

    for (int machineIndex = 0; machineIndex < machineBatches.size(); ++machineIndex) {
        MachineBatch& machineBatch = machineBatches[machineIndex];
        const Machine& machine = findMachineById(sortedMachines, machineBatch.MachineId);

        double currentRunningTime = machineBatch.RunningTime;

        for (int batchIndex = 0; batchIndex < machineBatch.Batches.size(); ++batchIndex) {

            Batch& batch = machineBatch.Batches[batchIndex];
            double finishTime = calculateFinishTime(batch, machineBatch.MachineId, &machine);

            if (canInsertPartToBatch(partInfo, batch, machine) && !canAddToBatchOrNeedNewBatch(batch, machine.MachineId, &machine) ||
                partInfo.orderInfo.DueDate >= finishTime) {

                MachineBatch tempMachineBatch = machineBatch;
                std::vector<PartTypeOrderInfo> tempParts; // 临时的vector用于存放单个partInfo
                tempParts.push_back(partInfo);
                tempMachineBatch.Batches[batchIndex].parts = tempParts;
                tempMachineBatch.Batches[batchIndex].totalArea += partInfo.partType->Area;
                updateMachineBatches(tempMachineBatch, sortedMachines);
                double additionalDelay = tempMachineBatch.TotalWeightedDelay - machineBatch.TotalWeightedDelay;
                if (currentRunningTime < leastRunningTime || (currentRunningTime == leastRunningTime && additionalDelay < bestAdditionalDelay)) {
                    bestAdditionalDelay = additionalDelay;
                    bestMachineIndex = machineIndex;
                    bestBatchIndex = batchIndex;
                    leastRunningTime = currentRunningTime;
                }
            }
        }
    }
    return std::make_tuple(bestMachineIndex, bestBatchIndex);
}

void insertPartAtPosition(MachineBatch& machineBatch, int batchIndex, const PartTypeOrderInfo& partInfo, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    // 如果是新批次，则初始化；否则，添加到现有批次
    if (batchIndex >= machineBatch.Batches.size()) {
        Batch newBatch;
        newBatch.batchId = partInfo.batchId; // 使用新零件的 PartTypeID 作为新批次的 ID
        newBatch.materialType = partInfo.Material;
        newBatch.totalArea = partInfo.partType->Area;
        newBatch.parts.push_back(partInfo); // 添加新零件
        machineBatch.Batches.push_back(newBatch);
    }
    else {
        Batch& targetBatch = machineBatch.Batches[batchIndex];
        targetBatch.batchId = std::max(targetBatch.batchId, partInfo.batchId);
        targetBatch.totalArea += partInfo.partType->Area;
        targetBatch.parts.push_back(partInfo);
    }

    // 更新机器批次的运行时间和总加权延迟
    updateMachineBatches(machineBatch, sortedMachines);
}

int findMachineBatchIndexByMachineId(const std::vector<MachineBatch>& machineBatches, int machineId) {
    auto it = std::find_if(machineBatches.begin(), machineBatches.end(),
        [machineId](const MachineBatch& mb) {
            return mb.MachineId == machineId;
        });
    if (it != machineBatches.end()) {
        return std::distance(machineBatches.begin(), it);
    }
    return -1; // 或者抛出异常
}
void printPartTypeOrderInfos(const std::vector<PartTypeOrderInfo>& partTypeOrderInfos, std::ofstream& outFile) {
    for (const auto& partInfo : partTypeOrderInfos) {
        outFile << "----------------------------------" << "\n";
        outFile << "MachineID: " << partInfo.machineID << "\n";
        outFile << "BatchID: " << partInfo.batchId << "\n";
        outFile << "Material: " << partInfo.Material << "\n";
        outFile << "Order Info - Order ID: " << partInfo.orderInfo.OrderId << "\n"; // 假设 OrderInfo 结构有一个 OrderId 成员
        if (partInfo.partType != nullptr) {
            // 假设 PartType 结构有 PartTypeId 和 Area 成员
            outFile << "Part Type Info - Part Type ID: " << partInfo.partType->PartTypeId << "\n";
            outFile << "Part Type Info - Area: " << partInfo.partType->Area << "\n";
        }
        else {
            outFile << "Part Type Info is nullptr" << "\n";
        }
        outFile << "----------------------------------" << "\n";
    }
}

void sortAndInsertParts(std::vector<MachineBatch>& machineBatches, const std::vector<std::pair<int, Machine>>& sortedMachines, std::vector<PartTypeOrderInfo>& parts) {
    // 对 parts 按照 DueDate, PenaltyCost, Volume 排序
    std::sort(parts.begin(), parts.end(), partComparator);
    for (auto& partInfo : parts) {
        bool inserted = false;

        // 先尝试找到最佳插入位置
        int bestMachineIndex, bestPosition;
        std::tie(bestMachineIndex, bestPosition) = findBestInsertionPosition(machineBatches, partInfo, sortedMachines);
        if (bestMachineIndex != -1 && bestPosition != -1) {
            MachineBatch& targetMachineBatch = machineBatches[bestMachineIndex];
            const Machine& targetMachine = findMachineById(sortedMachines, targetMachineBatch.MachineId);

            double newTotalArea = targetMachineBatch.Batches[bestPosition].totalArea + partInfo.partType->Area;
            if (newTotalArea <= targetMachine.Area) {
                insertPartAtPosition(targetMachineBatch, bestPosition, partInfo, sortedMachines);
                inserted = true;
            }
        }

        if (!inserted) {
            // 如果未插入，则选择运行时间最少的机台
            int machineIdToInsert = selectMachineWithLeastRunningTime(machineBatches);
            int machineIndexToInsert = findMachineBatchIndexByMachineId(machineBatches, machineIdToInsert);
            MachineBatch& machineBatchToInsert = machineBatches[machineIndexToInsert];
            const Machine& machineToInsert = findMachineById(sortedMachines, machineIdToInsert);

            if (partInfo.partType->Area <= machineToInsert.Area) {
                // 在运行时间最少的机台上创建新批次
                Batch newBatch;
                newBatch.materialType = partInfo.Material;
                newBatch.parts.push_back(partInfo);
                newBatch.totalArea = partInfo.partType->Area;
                machineBatchToInsert.Batches.push_back(newBatch); // 将新批次添加到机器批次中
                updateMachineBatches(machineBatchToInsert, sortedMachines);
            }
        }
    }
}



void printMachineBatch(const std::vector<MachineBatch> MachineBatchs, std::ofstream& outFile)
{
    for (const auto& machineBatch : MachineBatchs)
    {
        outFile << "Machine ID: " << machineBatch.MachineId << "\n";
        outFile << "Machine Area: " << machineBatch.MachineArea << "\n";
        outFile << "Running Time: " << machineBatch.RunningTime << "\n";
        outFile << "Total Weighted Delay: " << machineBatch.TotalWeightedDelay << "\n";
        outFile << "Batches:\n";

        for (const auto& batch : machineBatch.Batches)
        {
            outFile << "  Batch ID: " << batch.batchId << "\n";
            outFile << "  Batch Material Type: " << batch.materialType << "\n";
            outFile << "  Batch Total Area: " << batch.totalArea << "\n";
            outFile << "  Parts:\n";
            for (const auto& part : batch.parts)
            {
                outFile << "    Part Type ID: " << part.partType->PartTypeId << "\n";
                outFile << "    Order ID: " << part.orderInfo.OrderId << "\n";
            }
        }

        outFile << "Delayed Batches:\n";
        for (const auto& delayedBatch : machineBatch.delayedBatchInfo)
        {

            outFile << "  MachineID: " << machineBatch.MachineId << "\n";
            outFile << "  Batch Index: " << delayedBatch.BatchIndex << "\n";
            outFile << "  Weighted Delay: " << delayedBatch.WeightedDelay << "\n";
            outFile << "  Batch Time : " << delayedBatch.thisBatchTime << "\n";
        }

        outFile << "*******************************************" << "\n";
    }
}

double sumTotalWeightedDelay(const std::vector<MachineBatch>& machineBatches)
{
    double total = 0.0;
    for (const auto& batch : machineBatches)
    {
        total += batch.TotalWeightedDelay;
    }


    return total;
}

void printDelayedBatches(const std::vector<DelayedBatch>& delayedBatches, std::ofstream& outFile) {
    outFile << "Delayed Batches:\n";
    for (const auto& delayedBatch : delayedBatches) {
        outFile << "MachineID: " << delayedBatch.MachineId << "\n";
        outFile << "BatchID: " << delayedBatch.batch.batchId << "\n";
        outFile << "Batch Material Type: " << delayedBatch.batch.materialType << "\n";
        outFile << "Batch Total Area: " << delayedBatch.batch.totalArea << "\n";
        outFile << "Batch TIme: " << delayedBatch.time << "\n";
        outFile << "Parts:\n";
        for (const auto& part : delayedBatch.batch.parts) {
            outFile << "  Part Type ID: " << part.partType->PartTypeId << "\n";
            outFile << "  Order ID: " << part.orderInfo.OrderId << "\n";
        }
    }
}

// 方法1：交換兩個延遲批次
void method1(std::vector<MachineBatch>& machineBatches, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    // 随机数生成器初始化
    std::cout << "12.3.1" << std::endl;
    std::mt19937 rng(static_cast<unsigned int>(time(nullptr)));
    std::cout << "12.3.2" << std::endl;
    std::uniform_int_distribution<> distrib(0, machineBatches.size() - 1);
    std::cout << "12.3.3" << std::endl;
    // 随机选择两个不同的机器
    int machineIndex1 = distrib(rng);
    int machineIndex2 = distrib(rng);
    while (machineIndex2 == machineIndex1) {
        machineIndex2 = distrib(rng);
    }
    std::cout << "12.3.4" << std::endl;
    // 从每个机器中随机选择一个延迟批次
    std::uniform_int_distribution<> distribBatch1(0, machineBatches[machineIndex1].delayedBatchInfo.size() - 1);
    int batchIndex1 = distribBatch1(rng);
    std::cout << "12.3.5" << std::endl;
    std::uniform_int_distribution<> distribBatch2(0, machineBatches[machineIndex2].delayedBatchInfo.size() - 1);
    int batchIndex2 = distribBatch2(rng);
    std::cout << "12.3.6" << std::endl;
    // 交换批次位置
    if (machineIndex1 < machineBatches.size() && machineIndex2 < machineBatches.size()) {
        if (batchIndex1 < machineBatches[machineIndex1].Batches.size() && batchIndex2 < machineBatches[machineIndex2].Batches.size()) {
            // 安全地交换批次内容
            std::swap(machineBatches[machineIndex1].Batches[batchIndex1], machineBatches[machineIndex2].Batches[batchIndex2]);
        }
    }
    std::cout << "12.3.7" << std::endl;

    // 更新机器批次信息
    updateMachineBatches(machineBatches[machineIndex1], sortedMachines);
    updateMachineBatches(machineBatches[machineIndex2], sortedMachines);
    std::cout << "12.3.8" << std::endl;

}

std::random_device rd; // 用於產生非確定性隨機數
std::mt19937 rng(rd()); // 以隨機數種子初始化Mersenne Twister產生器

void method2(std::vector<MachineBatch>& machineBatches, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    std::vector<int> delayedBatchIndices;
    std::vector<int> nonDelayedBatchIndices;

    // 收集延遲和未延遲批次的索引
    for (int i = 0; i < machineBatches.size(); ++i) {
        for (int j = 0; j < machineBatches[i].Batches.size(); ++j) {
            if (std::none_of(machineBatches[i].delayedBatchInfo.begin(), machineBatches[i].delayedBatchInfo.end(),
                [j](const DelayedBatchInfo& dbi) { return dbi.BatchIndex == j; })) {
                nonDelayedBatchIndices.push_back(i * 1000 + j); // 使用 i * 1000 + j 來唯一標識每個批次
            }
            else {
                delayedBatchIndices.push_back(i * 1000 + j);
            }
        }
    }

    if (delayedBatchIndices.empty() || nonDelayedBatchIndices.empty()) {
        std::cout << "沒有找到適合交換的批次。" << std::endl;
        return;
    }

    // 隨機選擇一個延遲批次和一個未延遲批次
    std::uniform_int_distribution<int> delayedDist(0, delayedBatchIndices.size() - 1);
    std::uniform_int_distribution<int> nonDelayedDist(0, nonDelayedBatchIndices.size() - 1);

    int delayedIndex = delayedBatchIndices[delayedDist(rng)];
    int nonDelayedIndex = nonDelayedBatchIndices[nonDelayedDist(rng)];

    int delayedMachineIndex = delayedIndex / 1000;
    int delayedBatchIndex = delayedIndex % 1000;
    int nonDelayedMachineIndex = nonDelayedIndex / 1000;
    int nonDelayedBatchIndex = nonDelayedIndex % 1000;

    // 進行交換
    std::swap(machineBatches[delayedMachineIndex].Batches[delayedBatchIndex], machineBatches[nonDelayedMachineIndex].Batches[nonDelayedBatchIndex]);

    // 更新機器批次
    updateMachineBatches(machineBatches[delayedMachineIndex], sortedMachines);
    updateMachineBatches(machineBatches[nonDelayedMachineIndex], sortedMachines);

    std::cout << "成功交換並更新了批次。" << std::endl;
}


// 方法3：從延遲批次中抽取任一零件，插入到其他可行位置中
void method3(std::vector<MachineBatch>& machineBatches, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    std::srand(std::time(nullptr)); // 初始化随机数生成器

    // 随机选择一个含有延迟零件的批次
    std::vector<int> delayedMachineIndices;
    for (int i = 0; i < machineBatches.size(); ++i) {
        if (!machineBatches[i].delayedBatchInfo.empty()) {
            delayedMachineIndices.push_back(i);
        }
    }

    if (delayedMachineIndices.empty()) {
        std::cout << "没有找到含延遲零件的批次。" << std::endl;
        return;
    }

    int randomIndex = std::rand() % delayedMachineIndices.size();
    int machineIndex = delayedMachineIndices[randomIndex];
    MachineBatch& selectedMachineBatch = machineBatches[machineIndex];

    // 从选中的批次中随机选择一个零件
    int batchIndex = std::rand() % selectedMachineBatch.Batches.size();
    Batch& selectedBatch = selectedMachineBatch.Batches[batchIndex];
    if (selectedBatch.parts.empty()) {
        std::cout << "選中的批次没有零件。" << std::endl;
        return;
    }

    int partIndex = std::rand() % selectedBatch.parts.size();
    PartTypeOrderInfo selectedPart = selectedBatch.parts[partIndex];
    selectedBatch.parts.erase(selectedBatch.parts.begin() + partIndex);
    // updateMachineBatches(selectedMachineBatch, sortedMachines);

    // 寻找最佳插入位置
    int bestMachineIndex = -1, bestBatchIndex = -1;
    std::tie(bestMachineIndex, bestBatchIndex) = findBestInsertionPosition(machineBatches, selectedPart, sortedMachines);

    // 插入零件
    if (bestMachineIndex != -1 && bestBatchIndex != -1) {
        MachineBatch& targetMachineBatch = machineBatches[bestMachineIndex];
        Batch& targetBatch = targetMachineBatch.Batches[bestBatchIndex];

        targetBatch.parts.push_back(selectedPart);
        targetBatch.totalArea += selectedPart.partType->Area;

        updateMachineBatches(targetMachineBatch, sortedMachines);
    }
    else {
        std::cout << "没有找到適合的插入位置。" << std::endl;
    }
}

void executeRandomMethod(std::vector<MachineBatch>& machineBatches, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    std::cout << "12.1" << std::endl;
    std::srand(std::time(nullptr)); // 使用當前時間作為隨機數生成器的種子
    std::cout << "12.2" << std::endl;
    int method = std::rand() % 3 + 1; // 生成1至3之間的隨機數
    std::cout << "12.3" << std::endl;

    switch (method) {
    case 1:
        method1(machineBatches, sortedMachines);
        break;
    case 2:
        method2(machineBatches, sortedMachines);
        break;
    case 3:
        method3(machineBatches, sortedMachines);
        break;
    }
}
int calculateTotalSize(const std::map<int, std::vector<PartTypeOrderInfo>>& map) {
    int totalSize = 0;
    for (const auto& pair : map) {
        totalSize += pair.second.size(); // 加上每個vector的大小
    }
    return totalSize;
}


double step2(std::vector<MachineBatch>& tempMachineBatches, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    std::cout << "1" << std::endl;
    std::vector<DelayedBatch> delayedBatchesList = extractAndRandomSelectDelayedBatches(tempMachineBatches, sortedMachines);
    std::cout << "2" << std::endl;
    std::cout << "3" << std::endl;
    reintegrateDelayedBatches(tempMachineBatches, delayedBatchesList, sortedMachines);
    std::cout << "4" << std::endl;
    double currentResult = sumTotalWeightedDelay(tempMachineBatches);
    return currentResult;
}

double step3(std::vector<MachineBatch>& tempMachineBatches, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    std::cout << "5" << std::endl;
    std::vector<PartTypeOrderInfo> extractedParts = extractAndRandomSelectParts(tempMachineBatches);
    std::cout << "6" << std::endl;
    std::cout << "7" << std::endl;
    updateMachineBatchesAfterExtraction(tempMachineBatches, extractedParts, sortedMachines);
    std::cout << "8" << std::endl;
    std::cout << "9" << std::endl;
    sortAndInsertParts(tempMachineBatches, sortedMachines, extractedParts);
    std::cout << "10" << std::endl;

    double currentResult = sumTotalWeightedDelay(tempMachineBatches);
    return currentResult;
}
double step4(std::vector<MachineBatch>& tempMachineBatches, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    std::cout << "12" << std::endl;
    executeRandomMethod(tempMachineBatches, sortedMachines);
    std::cout << "13" << std::endl;
    double currentResult = sumTotalWeightedDelay(tempMachineBatches);
    return currentResult;
}


void read_json(const std::string& file_path, std::ofstream& outFile, std::ofstream& allTestFile)
{
    std::ifstream file(file_path);
    json j;
    file >> j;

    // 解析 Machines 部分
    std::map<int, Machine> machines;
    for (auto& kv : j["Machines"].items())
    {
        int key = std::stoi(kv.key());
        json value = kv.value();
        Machine m;
        m.MachineId = value["MachineId"];
        m.Area = value["Area"];
        m.Height = value["Height"];
        m.Length = value["Length"];
        m.Width = value["Width"];
        for (auto& material : value["Materials"])
        {
            m.Materials.push_back(material);
        }
        for (auto& setup : value["MaterialSetup"])
        {
            m.MaterialSetup.push_back(setup.get<std::vector<double>>());
        }
        // Parsing the StartSetup as a vector of doubles
        m.StartSetup = value["StartSetup"].get<std::vector<double>>();
        // Parsing the ScanTime, RecoatTime, and RemovalTime
        m.ScanTime = value["ScanTime"];
        m.RecoatTime = value["RecoatTime"];
        m.RemovalTime = value["RemovalTime"];
        machines[key] = m;
    }

    auto sortedMachines = sortMachines(machines); // 排序好的機器

    // 解析 PartTypes 部分
    std::map<int, PartType> partTypes;
    for (auto& kv : j["PartTypes"].items())
    {
        int key = std::stoi(kv.key());
        json value = kv.value();
        PartType p;
        p.PartTypeId = key;
        p.Height = value["Height"];
        p.Length = value["Length"];
        p.Width = value["Width"];
        p.Area = value["Area"];
        p.Volume = value["Volume"];
        partTypes[key] = p;
    }
    // 解析 Orders 部分
    std::map<int, Order> orders;
    std::map<int, std::vector<OrderDetail>> materialClassifiedOrderDetails;
    for (auto& kv : j["Orders"].items())
    {
        int key = std::stoi(kv.key());
        json value = kv.value();
        Order o;
        o.OrderId = value["OrderId"];
        o.DueDate = value["DueDate"];
        o.ReleaseDate = value["ReleaseDate"];
        o.PenaltyCost = value["PenaltyCost"];
        for (auto& detailValue : value["OrderList"])
        {
            OrderDetail od;
            int partTypeId = detailValue["PartType"];
            od.partType = &partTypes[partTypeId]; // Reference to PartType structure
            od.Quantity = detailValue["Quantity"];
            od.Material = detailValue["Material"];
            od.Quality = detailValue["Quality"];
            od.OrderId = o.OrderId; // 设置 OrderId
            materialClassifiedOrderDetails[od.Material].push_back(od);
            o.OrderList.push_back(od);
        }
        orders[o.OrderId] = o;
    }

    auto sortedMaterials = sortMaterialClassifiedOrderDetails(materialClassifiedOrderDetails, orders, partTypes);

    auto finalSorted = generateFinalSortedPartTypes(sortedMaterials, orders, partTypes);
    auto machineBatches = createMachineBatches(finalSorted, sortedMachines);

    printMachineBatch(machineBatches, outFile);

    outFile << "----------------------------------" << "\n";
    double result = sumTotalWeightedDelay(machineBatches);
    outFile << "  初始解 : " << result << "\n"; //step 6.
    outFile << "----------------------------------" << "\n";

    int machineSize = sortedMachines.size();
    int partSize = calculateTotalSize(finalSorted);


    auto bestMachineBatches = machineBatches;
    double bestResult = result; // 這裡使用深拷貝以確保完全獨立


    if (result != 0) {

        auto tempMachineBatches = bestMachineBatches;

        for (int i = 0;i < machineSize * partSize * 45;i++) {
            if (bestResult != 0) {
                double currentResult = step2(tempMachineBatches, sortedMachines);

                if (currentResult < bestResult) {
                    bestMachineBatches = tempMachineBatches;
                    bestResult = currentResult;
                    outFile << "第二步改進的解 : " << bestResult << "\n";
                }
                else {
                    outFile << "第二步保留之前的最佳解，當前解：" << currentResult << "\n";
                }

                if (currentResult == 0) {
                    continue; // 如果第二步結果為0，跳過後續步驟
                }

                // 進行第三步之前，基於當前最佳解（可能是從第一步或第二步保留下來的）
                tempMachineBatches = bestMachineBatches; // 確保第三步基於當前最佳解
                double currentResult2 = step3(tempMachineBatches, sortedMachines);

                if (currentResult2 < bestResult) {
                    bestMachineBatches = tempMachineBatches; // 如果第三步改進，更新最佳解
                    bestResult = currentResult2;
                    outFile << "第三步改進的解 : " << bestResult << "\n";
                }
                else {
                    outFile << "第三步保留之前的最佳解，當前解：" << currentResult2 << "\n";
                }

                if (currentResult2 == 0) {
                    continue; // 如果第三步結果為0，跳過後續步驟
                }

                tempMachineBatches = bestMachineBatches;
                double currentResult3 = step4(tempMachineBatches, sortedMachines);

                srand(static_cast<unsigned>(time(0)));
                double random_prob = static_cast<double>(rand()) / RAND_MAX;

                double m = ((currentResult3 - bestResult) / bestResult) * -100;
                double e_power_m = std::exp(m);

                if (currentResult3 < bestResult || random_prob <= e_power_m) {
                    bestMachineBatches = tempMachineBatches; // 如果第四步改進，更新最佳解
                    bestResult = currentResult3;
                    outFile << "第四步改進的解 : " << bestResult << "\n";
                }
                else {
                    outFile << "第四步保留之前的最佳解，當前解：" << currentResult3 << "\n";
                }
            }
        }
    }
    outFile << "**********************************" << "\n";
    printMachineBatch(bestMachineBatches, outFile);
    outFile << "結果 : " << "\n";
    outFile << "  初始解 : " << result << "\n";
    outFile << "  最佳解 : " << bestResult << "\n";
    outFile << "**********************************" << "\n";

    allTestFile << "檔案 名稱: " << file_path.substr(file_path.find_last_of("/\\") + 1) << "\n";
    allTestFile << "  初始解 : " << result << "\n";
    allTestFile << "  最佳解 : " << bestResult << "\n";
}


int main() {
    WIN32_FIND_DATAA findFileData;
    // HANDLE hFind = FindFirstFileA("C:/Users/2200555M/Documents/Project/test/*.json", &findFileData);
    // std::ofstream allTestFile("C:/Users/2200555M/Documents/Project/output/allTest.txt"); // 全局結果文件

    HANDLE hFind = FindFirstFileA("C:/Users/USER/Desktop/Project-main/test/*.json", &findFileData);
    std::ofstream allTestFile("C:/Users/USER/Desktop/Project-main/output/allTest.txt"); // 全局結果文件

    if (hFind == INVALID_HANDLE_VALUE) {
        std::cerr << "FindFirstFile failed\n";
        return 1;
    }
    else {
        do {
            std::string jsonFileName = std::string(findFileData.cFileName);
            // std::string fullPath = "C:/Users/2200555M/Documents/Project/test/" + jsonFileName;
            // std::string outputFileName = "C:/Users/2200555M/Documents/Project/output/output_" + jsonFileName + ".txt";

            std::string fullPath = "C:/Users/USER/Desktop/Project-main/test/" + jsonFileName;
            std::string outputFileName = "C:/Users/USER/Desktop/Project-main/output/output_" + jsonFileName + ".txt";

            std::ofstream outFile(outputFileName);

            read_json(fullPath, outFile, allTestFile); // 傳遞 allTestFile 給 read_json

            outFile.close();

        } while (FindNextFileA(hFind, &findFileData) != 0);
        FindClose(hFind);
    }

    allTestFile.close(); // 關閉全局結果文件

    return 0;
}