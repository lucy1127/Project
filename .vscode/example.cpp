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
    PartType* PartType;
    int Quantity;
    int Material;
    int Quality;
    int OrderId; // 添加 OrderId 成员变量
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
    PartType* PartType;
    OrderInfo OrderInfo;
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
};
struct MachineBatch
{
    int MachineId;
    double MachineArea;
    double RunningTime;
    double TotalWeightedDelay;
    std::vector<Batch> Batches;
    std::vector<DelayedBatchInfo> DelayedBatchInfo;
};

struct DelayedBatch {
    Batch batch;
    double time;
};


bool compareMachines(const std::pair<int, Machine>& a, const std::pair<int, Machine>& b)
{
    double totalTimeA = a.second.ScanTime + a.second.RecoatTime;
    double totalTimeB = b.second.ScanTime + b.second.RecoatTime;
    return totalTimeA < totalTimeB;
}

std::vector<std::pair<int, Machine>> sortMachines(const std::map<int, Machine>& machines)
{
    // 复制 machines map 到一个向量进行排序
    std::vector<std::pair<int, Machine>> machinesVec(machines.begin(), machines.end());

    // 使用自定义比较器排序向量
    std::sort(machinesVec.begin(), machinesVec.end(), compareMachines);

    return machinesVec;
}

bool orderDetailComparator(const OrderDetail& a, const OrderDetail& b, const std::map<int, Order>& orders, const std::map<int, PartType>& partTypes)
{
    const Order& orderA = orders.at(a.OrderId);
    const Order& orderB = orders.at(b.OrderId);
    const PartType& partA = *a.PartType;
    const PartType& partB = *b.PartType;

    // 首先根据 DueDate 排序
    if (orderA.DueDate != orderB.DueDate)
    {
        return orderA.DueDate < orderB.DueDate;
    }
    // 如果 DueDate 相同，则根据 PenaltyCost 排序
    if (orderA.PenaltyCost != orderB.PenaltyCost)
    {
        return orderA.PenaltyCost > orderB.PenaltyCost;
    }
    // 如果 PenaltyCost 也相同，则根据 Volume 排序
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

    // 從每個 Material 類型中收集所有 OrderDetail
    for (const auto& materialEntry : sortedMaterialOrderDetails)
    {
        std::vector<OrderDetail> allOrderDetails = materialEntry.second;

        // 根據自定義比較器進行排序
        std::sort(allOrderDetails.begin(), allOrderDetails.end(),
            [&](const OrderDetail& a, const OrderDetail& b)
            {
                return orderDetailComparator(a, b, orders, partTypes);
            });

        // 遍歷排序後的 OrderDetail
        for (const auto& detail : allOrderDetails)
        {
            PartTypeOrderInfo info;
            info.PartType = detail.PartType; // 假設 OrderDetail 中有 PartTypeId
            info.Material = detail.Material; // 假設 OrderDetail 中有 MaterialId
            const Order& order = orders.at(detail.OrderId);

            // 创建 OrderInfo 并填充数据
            OrderInfo orderInfo;
            orderInfo.OrderId = order.OrderId;
            orderInfo.DueDate = order.DueDate;
            orderInfo.ReleaseDate = order.ReleaseDate;
            orderInfo.PenaltyCost = order.PenaltyCost;
            info.OrderInfo = orderInfo;

            // 按 Material 分類添加到最終結果
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
            if (partInfo.OrderInfo.DueDate < earliestDueDate ||
                (partInfo.OrderInfo.DueDate == earliestDueDate && partInfo.OrderInfo.PenaltyCost < lowestPenaltyCost) ||
                (partInfo.OrderInfo.DueDate == earliestDueDate && partInfo.OrderInfo.PenaltyCost == lowestPenaltyCost && partInfo.PartType->Volume > smallestVolume))
            {

                earliestDueDate = partInfo.OrderInfo.DueDate;
                lowestPenaltyCost = partInfo.OrderInfo.PenaltyCost;
                smallestVolume = partInfo.PartType->Volume;
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

double calculateFinishTime(const Batch& batch, const int& machineId, const std::vector<std::pair<int, Machine>>& sortedMachines)
{
    double volumeTime = 0.0;
    double maxHeight = 0.0;

    // 找到對應的機器
    const Machine* machine = nullptr;
    for (const auto& machinePair : sortedMachines)
    {
        if (machinePair.first == machineId)
        {
            machine = &machinePair.second;
            break;
        }
    }

    if (!machine)
    {
        throw std::runtime_error("Machine with the specified ID not found.");
    }

    for (const auto& partInfo : batch.parts)
    {
        volumeTime += partInfo.PartType->Volume * machine->ScanTime;
        maxHeight = std::max(maxHeight, partInfo.PartType->Height);
    }

    double heightTime = maxHeight * machine->RecoatTime;

    return std::max(volumeTime, heightTime);
}

Batch allocateMaterialToMachine(MachineBatch& selectedMachineBatch, int selectedMaterial, std::map<int, std::vector<PartTypeOrderInfo>>& remainingMaterials)
{
    Batch newBatch;
    newBatch.materialType = selectedMaterial;
    newBatch.totalArea = 0.0;

    if (remainingMaterials.find(selectedMaterial) != remainingMaterials.end())
    {
        auto& materialList = remainingMaterials[selectedMaterial];
        auto it = materialList.begin();
        while (it != materialList.end())
        {
            double partArea = it->PartType->Area;
            if (newBatch.totalArea + partArea <= selectedMachineBatch.MachineArea)
            {
                newBatch.parts.push_back(*it);
                newBatch.totalArea += partArea;
                it = materialList.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    return newBatch;
}

void updateRemainingMaterials(std::map<int, std::vector<PartTypeOrderInfo>>& remainingMaterials, int selectedMaterialType)
{
    // 檢查選定的材料類型是否存在於 remainingMaterials 中
    if (remainingMaterials.find(selectedMaterialType) != remainingMaterials.end())
    {
        // 移除已分配的材料
        // 如果材料的陣列為空，則從映射中刪除該材料類型
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
            const auto& orderInfo = materialEntry.second.front().OrderInfo;
            const auto& partType = materialEntry.second.front().PartType;
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

void initializeMachines(std::vector<MachineBatch>& machineBatches, const std::vector<std::pair<int, Machine>>& sortedMachines, int selectedMaterial, const std::map<int, std::vector<PartTypeOrderInfo>>& sortedMaterials)
{
    for (const auto& machinePair : sortedMachines)
    {
        MachineBatch machineBatch;
        machineBatch.MachineId = machinePair.first;
        machineBatch.MachineArea = machinePair.second.Area;
        // 假設 StartSetup 是 Machine 的一部分並且與材料類型相關
        double setupTime = machinePair.second.StartSetup[selectedMaterial];
        machineBatch.RunningTime = setupTime; // 設置初始運行時間
        std::cout << "setupTime: " << machineBatch.RunningTime << std::endl;
        machineBatches.push_back(machineBatch);
    }
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
        double delayTime = runningTime - partInfo.OrderInfo.DueDate;
        if (delayTime > 0)
        {

            weightedDelay += delayTime * partInfo.OrderInfo.PenaltyCost;
        }
    }

    return weightedDelay;
}

std::vector<MachineBatch> createMachineBatches(
    const std::map<int, std::vector<PartTypeOrderInfo>>& sortedMaterials,
    const std::vector<std::pair<int, Machine>>& sortedMachines)
{
    std::vector<MachineBatch> machineBatches;
    std::map<int, int> lastMaterialForMachine;

    // Initialize machines
    std::cout << "Initializing machines...\n";
    for (const auto& machinePair : sortedMachines)
    {
        machineBatches.emplace_back(MachineBatch{
            machinePair.first,
            machinePair.second.Area,
            0.0, // RunningTime
            0.0  // TotalWeightedDelay
            });

        std::cout << "Initialized machine ID: " << machinePair.first << " with Area: " << machinePair.second.Area << "\n";
    }

    auto remainingMaterials = sortedMaterials;

    while (!remainingMaterials.empty())
    {
        int selectedMaterial = selectEarliestMaterial(remainingMaterials);
        std::cout << "selectedMaterial : " << selectedMaterial << std::endl;

        int selectedMachineId = selectMachineWithLeastRunningTime(machineBatches);
        MachineBatch& machineBatchRef = findMachineBatchById(machineBatches, selectedMachineId);

        auto machineIt = std::find_if(sortedMachines.begin(), sortedMachines.end(),
            [selectedMachineId](const std::pair<int, Machine>& element)
            { return element.second.MachineId == selectedMachineId; });

        double setUpTime = 0.0;

        if (machineIt != sortedMachines.end()) {
            if (lastMaterialForMachine.find(selectedMachineId) == lastMaterialForMachine.end() ||
                lastMaterialForMachine[selectedMachineId] != selectedMaterial) {

                setUpTime = (lastMaterialForMachine.find(selectedMachineId) == lastMaterialForMachine.end()) ?
                    machineIt->second.StartSetup[selectedMaterial] :
                    std::accumulate(machineIt->second.MaterialSetup[selectedMaterial].begin(),
                        machineIt->second.MaterialSetup[selectedMaterial].end(),
                        0.0);

                lastMaterialForMachine[selectedMachineId] = selectedMaterial;
            }

            std::cout << "Machine first SetUp: " << machineIt->second.StartSetup[selectedMaterial] << std::endl;
            std::cout << "Machine Total MaterialSetUP: " << std::accumulate(machineIt->second.MaterialSetup[selectedMaterial].begin(),
                machineIt->second.MaterialSetup[selectedMaterial].end(),
                0.0) << std::endl;
            std::cout << "Different Material: " << std::endl;
            std::cout << "setUpTime: " << setUpTime << " for machine ID: " << selectedMachineId << std::endl;
            std::cout << "----------------------------------------------------------------------------- " << std::endl;

            machineBatchRef.RunningTime += setUpTime;
        }

        Batch newBatch = allocateMaterialToMachine(machineBatchRef, selectedMaterial, remainingMaterials);
        double finishTime = calculateFinishTime(newBatch, machineBatchRef.MachineId, sortedMachines);
        machineBatchRef.RunningTime += finishTime;
        std::cout << "----------------------------------------------------------------------------- " << std::endl;
        std::cout << "Machine: " << selectedMachineId << std::endl;
        std::cout << "BatchfinishTime: " << finishTime << std::endl;
        std::cout << "RunningTime: " << machineBatchRef.RunningTime << std::endl;
        std::cout << "----------------------------------------------------------------------------- " << std::endl;
        machineBatchRef.Batches.push_back(newBatch);

        DelayedBatchInfo delayedInfo{
            static_cast<int>(machineBatchRef.Batches.size()),
            calculateWeightedDelay(newBatch, machineBatchRef.RunningTime),
            machineBatchRef.RunningTime
        };

        machineBatchRef.DelayedBatchInfo.push_back(delayedInfo);
        machineBatchRef.TotalWeightedDelay += delayedInfo.WeightedDelay;

        updateRemainingMaterials(remainingMaterials, selectedMaterial);
        lastMaterialForMachine[selectedMachineId] = selectedMaterial;
    }

    return machineBatches;
}

// 函数以抽取并返回所有延迟的批次
std::vector<DelayedBatch> extractDelayedBatches(std::vector<MachineBatch>& machineBatches) {
    std::vector<DelayedBatch> delayedBatches;

    for (auto& machineBatch : machineBatches) {
        // 逆序遍历，从最后一个延迟批次开始
        for (int i = machineBatch.DelayedBatchInfo.size() - 1; i >= 0; i--) {
            const auto& delayedInfo = machineBatch.DelayedBatchInfo[i];
            if (delayedInfo.WeightedDelay > 0) {  // 如果该批次延迟
                DelayedBatch delayedBatch;
                delayedBatch.batch = machineBatch.Batches[delayedInfo.BatchIndex - 1]; // BatchIndex 是从 1 开始的
                delayedBatch.time = machineBatch.DelayedBatchInfo[delayedInfo.BatchIndex - 1].thisBatchTime;
                delayedBatches.push_back(delayedBatch);

                // 从原始机器批次中移除该批次
                machineBatch.Batches.erase(machineBatch.Batches.begin() + delayedInfo.BatchIndex - 1);
            }
        }
    }
    return delayedBatches;
}


// 更新 MachineBatch 数据
void updateMachineBatches(MachineBatch& machineBatch, const std::vector<std::pair<int, Machine>>& sortedMachines) {

    machineBatch.RunningTime = 0;
    machineBatch.TotalWeightedDelay = 0;

    for (const auto& batch : machineBatch.Batches) {
        double finishTime = calculateFinishTime(batch, machineBatch.MachineId, sortedMachines);
        machineBatch.RunningTime += finishTime;
        double batchDelay = calculateWeightedDelay(batch, machineBatch.RunningTime);
        machineBatch.TotalWeightedDelay += batchDelay;
    }

    machineBatch.DelayedBatchInfo.clear();

}


bool compareDelayedBatches(const DelayedBatch& a, const DelayedBatch& b) {
    const auto& partA = a.batch.parts.front().OrderInfo;
    const auto& partB = b.batch.parts.front().OrderInfo;

    const auto& AreaA = a.batch.parts.front().PartType->Area;
    const auto& AreaB = b.batch.parts.front().PartType->Area;

    if (partA.DueDate != partB.DueDate)
        return partA.DueDate < partB.DueDate;
    if (partA.PenaltyCost != partB.PenaltyCost)
        return partA.PenaltyCost > partB.PenaltyCost;
    return AreaA > AreaB;
}

// 尝试将延迟批次插入到机器批次中，并返回新的总延迟
double tryInsertBatch(MachineBatch& machineBatch, const Batch& batchToInsert, int position, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    // 假设我们有一个函数来计算批次完成时间
    auto finishTime = calculateFinishTime(batchToInsert, machineBatch.MachineId, sortedMachines);

    // 插入批次
    machineBatch.Batches.insert(machineBatch.Batches.begin() + position, batchToInsert);

    // 计算新的总延迟
    double newTotalDelay = 0.0;
    for (const auto& batch : machineBatch.Batches) {
        newTotalDelay += calculateWeightedDelay(batch, machineBatch.RunningTime);
        machineBatch.RunningTime += finishTime; // 假设finishTime已经计算
    }

    // 移除插入的批次，因为我们只是在测试插入的效果
    machineBatch.Batches.erase(machineBatch.Batches.begin() + position);

    return newTotalDelay;
}

void reintegrateDelayedBatches(std::vector<MachineBatch>& machineBatches, std::vector<DelayedBatch>& delayedBatches, const std::vector<std::pair<int, Machine>>& sortedMachines) {
    // 按照定义的准则对延迟批次进行排序
    std::sort(delayedBatches.begin(), delayedBatches.end(), compareDelayedBatches);

    // 处理每个延迟批次
    for (auto& delayedBatch : delayedBatches) {
        double bestAdditionalDelay = std::numeric_limits<double>::max();
        MachineBatch* bestMachineBatch = nullptr;
        int bestPosition = -1;

        // 尝试在每个机器上找到最佳插入点
        for (auto& machineBatch : machineBatches) {
            for (size_t i = 0; i <= machineBatch.Batches.size(); ++i) {
                double trialDelay = tryInsertBatch(machineBatch, delayedBatch.batch, i, sortedMachines);
                double additionalDelay = trialDelay - machineBatch.TotalWeightedDelay;

                if (additionalDelay < bestAdditionalDelay) {
                    bestAdditionalDelay = additionalDelay;
                    bestMachineBatch = &machineBatch;
                    bestPosition = i;
                }
            }
        }

        // 如果找到了最佳位置，将批次插入到机器批次中
        if (bestMachineBatch && bestPosition >= 0) {
            bestMachineBatch->Batches.insert(bestMachineBatch->Batches.begin() + bestPosition, delayedBatch.batch);
            // 更新机器的总延迟和运行时间
            bestMachineBatch->TotalWeightedDelay += bestAdditionalDelay;

            // 正确调用 updateMachineBatches，传递*bestMachineBatch和sortedMachines
            updateMachineBatches(*bestMachineBatch, sortedMachines);
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
                outFile << "    Part Type ID: " << part.PartType->PartTypeId << "\n";
                outFile << "    Order ID: " << part.OrderInfo.OrderId << "\n";
            }
        }

        outFile << "Delayed Batches:\n";
        for (const auto& delayedBatch : machineBatch.DelayedBatchInfo)
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
            outFile << "  Part Type ID: " << part.PartType->PartTypeId << "\n";
            outFile << "  Order ID: " << part.OrderInfo.OrderId << "\n";
        }
    }
}
void read_json(const std::string& file_path, std::ofstream& outFile)
{
    std::ifstream file(file_path);
    json j;
    file >> j;

    // Read basic parameters
    int instanceID = j["InstanceID"];
    int seedValue = j["SeedValue"];
    int nOrders = j["nOrders"];
    int nItemsPerOrder = j["nItemsPerOrder"];
    int nTotalItems = j["nTotalItems"];
    int nMachines = j["nMachines"];
    int nMaterials = j["nMaterials"];
    double PercentageOfTardyOrders = j["PercentageOfTardyOrders"];
    double DueDateRange = j["DueDateRange"];

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
            od.PartType = &partTypes[partTypeId]; // Reference to PartType structure
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



    std::vector<DelayedBatch> delayedBatches = extractDelayedBatches(machineBatches);
    printDelayedBatches(delayedBatches, outFile);
    // 更新每台机器的运行时间和总延迟
    for (auto& machineBatch : machineBatches) {
        updateMachineBatches(machineBatch, sortedMachines);
    }
    outFile << "----------------------------------" << "\n";
    printMachineBatch(machineBatches, outFile);


    //TODO：把抽取出來的 delayedBatches 重新插入 機台中 
    // 1. 先比 DelayedBatch 中 哪個Batch中第一個 的Dudate 最早(小), 一樣就比 pentycost 最大 ,一樣就比Area 最大
    // 圖例 : <4,5> , <17,18> ,<29,30> // 延遲批次
    // <29,30> 為最先排序 
    // 2. <29,30>的  Material Type: 0 ,所以只能插入 在  Batch 為 Material Type: 0 的前後 
    //所以最下面的圖為 能 插入的批次的可能 
    //3. 每種可能的機台 重新計算批次延遲時間  
    //4. 選擇延遲最少的那台 做插入
    //5. 依照以上步驟完成 delayedBatches 的插入 
    // (可以參考使用以上寫好的FUNCTION )

    reintegrateDelayedBatches(machineBatches, delayedBatches, sortedMachines);
    outFile << "----------------------------------" << "\n";
    outFile << " 插入後 : " << "\n";
    printMachineBatch(machineBatches, outFile);
    // outFile << "----------------------------------" << "\n";
    // double result2 = sumTotalWeightedDelay(machineBatches);
    // outFile << "  第2次初始解 : " << result2 << "\n"; //step 6.
    // outFile << "----------------------------------" << "\n";



}


int main() {
    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA("C:/Users/2200555M/Documents/Project/.vscode/test/*.json", &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        std::cerr << "FindFirstFile failed\n";
        return 1;
    }
    else {
        do {
            std::string jsonFileName = std::string(findFileData.cFileName);
            std::string fullPath = "C:/Users/2200555M/Documents/Project/.vscode/test/" + jsonFileName;


            std::string outputFileName = "output_" + jsonFileName + ".txt";
            std::ofstream outFile(outputFileName);

            read_json(fullPath, outFile);

            outFile.close();

        } while (FindNextFileA(hFind, &findFileData) != 0);
        FindClose(hFind);
    }
    return 0;
}
