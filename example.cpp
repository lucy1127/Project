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
    int Material;
    PartType* partType;
    OrderInfo orderInfo;
};

struct Batch
{
    int materialType;
    std::vector<PartTypeOrderInfo> parts;
    double totalArea;
};
struct DelayedBatchInfo
{
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

AllocationResult allocateMaterialToMachine(MachineBatch& selectedMachineBatch, int selectedMaterial,
    std::map<int, std::vector<PartTypeOrderInfo>>& remainingMaterials,
    const Machine* machine) {
    Batch newBatch;
    newBatch.materialType = selectedMaterial;
    newBatch.totalArea = 0.0;

    if (remainingMaterials.find(selectedMaterial) != remainingMaterials.end()) {
        auto& materialList = remainingMaterials[selectedMaterial];
        for (auto it = materialList.begin(); it != materialList.end();) {
            double partArea = it->partType->Area;


            if (newBatch.totalArea + partArea <= selectedMachineBatch.MachineArea) {

                newBatch.parts.push_back(*it);
                newBatch.totalArea += partArea;


                double finishTime = calculateFinishTime(newBatch, selectedMachineBatch.MachineId, machine);


                if (canAddToBatchOrNeedNewBatch(newBatch, selectedMachineBatch.MachineId, machine) &&
                    it->orderInfo.DueDate < finishTime) {

                    it = materialList.erase(it);
                }
                else {

                    newBatch.parts.pop_back();
                    newBatch.totalArea -= partArea;


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

        if (newBatch.parts.empty() && !materialList.empty()) {
            newBatch.parts.push_back(materialList.front());
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
            static_cast<int>(machineBatchRef.Batches.size()),
            calculateWeightedDelay(result.batch, machineBatchRef.RunningTime),
            machineBatchRef.RunningTime
        };

        machineBatchRef.delayedBatchInfo.push_back(delayedInfo);
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

    for (const auto& batch : machineBatch.Batches) {
        double finishTime = calculateFinishTime(batch, machineBatch.MachineId, machine);
        machineBatch.RunningTime += finishTime;
        double batchDelay = calculateWeightedDelay(batch, machineBatch.RunningTime);
        machineBatch.TotalWeightedDelay += batchDelay;

        if (batchDelay > 0) {
            DelayedBatchInfo dbInfo;
            dbInfo.BatchIndex = &batch - &machineBatch.Batches[0] + 1;
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
    tempMachineBatch.Batches.insert(tempMachineBatch.Batches.begin() + position, batchToInsert);

    tempMachineBatch.RunningTime = 0.0;
    tempMachineBatch.TotalWeightedDelay = 0.0;

    int lastMaterial = -1;
    for (size_t i = 0; i < tempMachineBatch.Batches.size(); ++i) {
        const Batch& currentBatch = tempMachineBatch.Batches[i];
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




std::vector<DelayedBatch> extractRandomBatchesIncludingMaxDelay(const std::vector<DelayedBatch>& delayedBatches) {
    if (delayedBatches.empty()) {
        return {};
    }

    auto maxDelayIt = std::max_element(delayedBatches.begin(), delayedBatches.end(),
        [](const DelayedBatch& a, const DelayedBatch& b) {
            return a.WeightedDelay < b.WeightedDelay;
        });


    DelayedBatch maxDelayBatch = *maxDelayIt;


    std::vector<DelayedBatch> tempBatches = delayedBatches;
    auto maxDelayItCopy = tempBatches.begin() + (maxDelayIt - delayedBatches.begin());
    tempBatches.erase(maxDelayItCopy);


    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(tempBatches.begin(), tempBatches.end(), g);

    std::uniform_int_distribution<size_t> dist(1, tempBatches.size() + 1); // +1 to include the possibility of only selecting the maxDelayBatch
    size_t numBatchesToExtract = dist(g);

    std::vector<DelayedBatch> extractedBatches;
    extractedBatches.push_back(maxDelayBatch);

    // Add random batches up to the determined number
    for (size_t i = 0; i < numBatchesToExtract - 1; ++i) { // -1 because we've already added the max delay batch
        extractedBatches.push_back(tempBatches[i]);
    }

    return extractedBatches;
}


std::vector<PartTypeOrderInfo> extractAndRandomSelectParts(std::vector<MachineBatch>& machineBatches) {
    std::vector<PartTypeOrderInfo> allDelayedParts;
    DelayedBatch maxDelayBatch;
    double maxWeightedDelay = 0;

    // 從 machineBatches 中提取所有延遲零件
    for (auto& machineBatch : machineBatches) {
        for (auto& delayedInfo : machineBatch.delayedBatchInfo) {
            if (delayedInfo.WeightedDelay > 0) {
                Batch& delayedBatch = machineBatch.Batches[delayedInfo.BatchIndex];
                // 添加調試信息

                allDelayedParts.insert(allDelayedParts.end(), delayedBatch.parts.begin(), delayedBatch.parts.end());

                if (delayedInfo.WeightedDelay > maxWeightedDelay) {
                    maxWeightedDelay = delayedInfo.WeightedDelay;
                    maxDelayBatch = { delayedBatch, delayedInfo.WeightedDelay };
                }
            }
        }
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<int> dist(1, allDelayedParts.size());
    int beta = dist(g);

    // 從加權延遲最大批次中選擇延遲零件
    std::vector<PartTypeOrderInfo> selectedParts;
    if (!maxDelayBatch.batch.parts.empty()) {
        selectedParts.push_back(maxDelayBatch.batch.parts.front());
    }

    // 從所有延遲零件中隨機選擇其餘零件
    std::shuffle(allDelayedParts.begin(), allDelayedParts.end(), g);

    while (selectedParts.size() < beta && !allDelayedParts.empty()) {
        selectedParts.push_back(allDelayedParts.back());
        allDelayedParts.pop_back();
    }

    return selectedParts;
}

void updateMachineBatchesAfterExtraction(std::vector<MachineBatch>& machineBatches, const std::vector<PartTypeOrderInfo>& extractedParts, const std::vector<std::pair<int, Machine>>& sortedMachines) {

    std::set<int> batchesToRemove;

    for (auto& machineBatch : machineBatches) {
        for (int i = 0; i < machineBatch.Batches.size(); ++i) {
            Batch& batch = machineBatch.Batches[i];

            for (auto& part : batch.parts) {
                if (std::find_if(extractedParts.begin(), extractedParts.end(), [&part](const PartTypeOrderInfo& extractedPart) {
                    return extractedPart.partType->PartTypeId == part.partType->PartTypeId &&
                        extractedPart.orderInfo.OrderId == part.orderInfo.OrderId;
                    }) != extractedParts.end()) {
                    batchesToRemove.insert(i);
                    break;
                }
            }
        }

        for (auto it = batchesToRemove.rbegin(); it != batchesToRemove.rend(); ++it) {
            machineBatch.Batches.erase(machineBatch.Batches.begin() + *it);
        }

        batchesToRemove.clear();

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

    for (int machineIndex = 0; machineIndex < machineBatches.size(); ++machineIndex) {
        MachineBatch& machineBatch = machineBatches[machineIndex];
        const Machine& machine = findMachineById(sortedMachines, machineBatch.MachineId);

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

// 在找到的最佳批次位置插入零件
void insertPartAtPosition(MachineBatch& machineBatch, int batchIndex, const PartTypeOrderInfo& partInfo, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    // 在批次中添加部件
    std::vector<PartTypeOrderInfo> tempParts; // 临时的vector用于存放单个partInfo
    tempParts.push_back(partInfo);
    machineBatch.Batches[batchIndex].parts = tempParts;
    machineBatch.Batches[batchIndex].totalArea += partInfo.partType->Area;

    // 更新机器批次的运行时间和总加权延迟
    updateMachineBatches(machineBatch, sortedMachines);

    // 如果新的加权延迟大于0，则更新 DelayedBatchInfo
    double batchDelay = calculateWeightedDelay(machineBatch.Batches[batchIndex], machineBatch.RunningTime);
    if (batchDelay > 0) {
        DelayedBatchInfo dbInfo;
        dbInfo.BatchIndex = batchIndex + 1;  // 批次索引从1开始
        dbInfo.WeightedDelay = batchDelay;
        dbInfo.thisBatchTime = machineBatch.RunningTime;
        dbInfo.newPosition = batchIndex;  // 插入的位置
        machineBatch.delayedBatchInfo.push_back(dbInfo);
    }
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
void printPartTypeOrderInfos(const std::vector<PartTypeOrderInfo>& partTypeOrderInfos) {
    for (const auto& partInfo : partTypeOrderInfos) {
        std::cout << "Material: " << partInfo.Material << std::endl;
        std::cout << "Order Info - Order ID: " << partInfo.orderInfo.OrderId << std::endl; // 假设 OrderInfo 结构有一个 OrderId 成员
        if (partInfo.partType != nullptr) {
            // 假设 PartType 结构有 PartTypeId 和 Area 成员
            std::cout << "Part Type Info - Part Type ID: " << partInfo.partType->PartTypeId << std::endl;
            std::cout << "Part Type Info - Area: " << partInfo.partType->Area << std::endl;
        }
        else {
            std::cout << "Part Type Info is nullptr" << std::endl;
        }
        std::cout << "----------------------------------" << std::endl;
    }
}

void sortAndInsertParts(std::vector<MachineBatch>& machineBatches, const std::vector<std::pair<int, Machine>>& sortedMachines, std::vector<PartTypeOrderInfo>& parts) {
    // 对 parts 按照 DueDate, PenaltyCost, Volume 排序
    std::cout << "4.1" << std::endl;
    std::sort(parts.begin(), parts.end(), partComparator);
    std::cout << "4.2" << std::endl;
    for (auto& partInfo : parts) {

        int bestMachineIndex, bestPosition;
        std::tie(bestMachineIndex, bestPosition) = findBestInsertionPosition(machineBatches, partInfo, sortedMachines);
        std::cout << "4.3" << std::endl;
        if (bestMachineIndex != -1 && bestPosition != -1) {

            insertPartAtPosition(machineBatches[bestMachineIndex], bestPosition, partInfo, sortedMachines);
            std::cout << "4.4" << std::endl;
        }
        else {
            int machineIdToInsert = selectMachineWithLeastRunningTime(machineBatches);
            int machineIndexToInsert = findMachineBatchIndexByMachineId(machineBatches, machineIdToInsert);

            MachineBatch& machineBatchToInsert = machineBatches[machineIndexToInsert];
            Batch newBatch;

            newBatch.materialType = partInfo.Material;
            std::vector<PartTypeOrderInfo> tempParts; // 临时的vector用于存放单个partInfo
            tempParts.push_back(partInfo);
            newBatch.parts = tempParts;
            newBatch.totalArea = partInfo.partType->Area;// 新批次的总面积等于零件的面积
            std::cout << "4.5" << std::endl;
            const Machine& machineToInsert = findMachineById(sortedMachines, machineIdToInsert);
            if (newBatch.totalArea <= machineToInsert.Area) {
                machineBatchToInsert.Batches.push_back(newBatch); // 将新批次添加到机器批次中
                updateMachineBatches(machineBatchToInsert, sortedMachines); // 更新机器批次信息
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


void read_json(const std::string& file_path, std::ofstream& outFile)
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

    if (result != 0) {

        std::vector<DelayedBatch> delayedBatches = extractDelayedBatches(machineBatches);
        printDelayedBatches(delayedBatches, outFile);
        // 更新每台機器的運行時間和總延遲
        for (auto& machineBatch : machineBatches) {
            updateMachineBatches(machineBatch, sortedMachines);
        }
        outFile << "----------------------------------" << "\n";
        printMachineBatch(machineBatches, outFile);
        std::vector<DelayedBatch> delayedBatchesList = extractRandomBatchesIncludingMaxDelay(delayedBatches);
        outFile << "隨機抽取 : " << "\n";
        outFile << "----------------------------------" << "\n";
        printDelayedBatches(delayedBatchesList, outFile);

        outFile << "----------------------------------" << "\n";
        reintegrateDelayedBatches(machineBatches, delayedBatchesList, sortedMachines);

        outFile << "----------------------------------" << "\n";
        outFile << " 插入後 : " << "\n";
        outFile << "----------------------------------" << "\n";
        printMachineBatch(machineBatches, outFile);
        std::cout << "1" << std::endl;
        outFile << "----------------------------------" << "\n";
        double result2 = sumTotalWeightedDelay(machineBatches);
        outFile << "  第2次初始解 : " << result2 << "\n";
        outFile << "----------------------------------" << "\n";
        std::cout << "2" << std::endl;
        //TODO：

        std::vector<PartTypeOrderInfo> extractedParts = extractAndRandomSelectParts(machineBatches);
        std::cout << "3" << std::endl;
        updateMachineBatchesAfterExtraction(machineBatches, extractedParts, sortedMachines);
        std::cout << "4" << std::endl;
        sortAndInsertParts(machineBatches, sortedMachines, extractedParts);
        std::cout << "5" << std::endl;
        printMachineBatch(machineBatches, outFile);
        outFile << "----------------------------------" << "\n";
        double result3 = sumTotalWeightedDelay(machineBatches);
        outFile << "  第3次初始解 : " << result3 << "\n";
        outFile << "----------------------------------" << "\n";

    }


}


int main() {
    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA("C:/Users/2200555M/Documents/Project/test/*.json", &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        std::cerr << "FindFirstFile failed\n";
        return 1;
    }
    else {
        do {
            std::string jsonFileName = std::string(findFileData.cFileName);
            std::string fullPath = "C:/Users/2200555M/Documents/Project/test/" + jsonFileName;

            std::string outputFileName = "C:/Users/2200555M/Documents/Project/output/output_" + jsonFileName + ".txt";
            std::ofstream outFile(outputFileName);

            read_json(fullPath, outFile);

            outFile.close();

        } while (FindNextFileA(hFind, &findFileData) != 0);
        FindClose(hFind);
    }
    return 0;
}

