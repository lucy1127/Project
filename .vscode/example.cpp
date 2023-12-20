#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>
#include <unordered_set>
#include <utility>


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
    PartType *PartType;
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
    PartType *PartType;
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

bool compareMachines(const std::pair<int, Machine> &a, const std::pair<int, Machine> &b)
{
    double totalTimeA = a.second.ScanTime + a.second.RecoatTime;
    double totalTimeB = b.second.ScanTime + b.second.RecoatTime;
    return totalTimeA < totalTimeB;
}

std::vector<std::pair<int, Machine>> sortMachines(const std::map<int, Machine> &machines)
{
    // 复制 machines map 到一个向量进行排序
    std::vector<std::pair<int, Machine>> machinesVec(machines.begin(), machines.end());

    // 使用自定义比较器排序向量
    std::sort(machinesVec.begin(), machinesVec.end(), compareMachines);

    return machinesVec;
}

bool orderDetailComparator(const OrderDetail &a, const OrderDetail &b, const std::map<int, Order> &orders, const std::map<int, PartType> &partTypes)
{
    const Order &orderA = orders.at(a.OrderId);
    const Order &orderB = orders.at(b.OrderId);
    const PartType &partA = *a.PartType;
    const PartType &partB = *b.PartType;

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
    const std::map<int, std::vector<OrderDetail>> &materialClassifiedOrderDetails,
    const std::map<int, Order> &orders,
    const std::map<int, PartType> &partTypes)
{
    std::map<int, std::vector<OrderDetail>> sortedMaterialOrderDetails;

    for (const auto &entry : materialClassifiedOrderDetails)
    {
        auto sortedOrderDetails = entry.second;

        std::sort(sortedOrderDetails.begin(), sortedOrderDetails.end(),
                  [&](const OrderDetail &a, const OrderDetail &b)
                  {
                      return orderDetailComparator(a, b, orders, partTypes);
                  });

        sortedMaterialOrderDetails[entry.first] = sortedOrderDetails;
    }

    return sortedMaterialOrderDetails;
}

std::map<int, std::vector<PartTypeOrderInfo>> generateFinalSortedPartTypes(
    const std::map<int, std::vector<OrderDetail>> &sortedMaterialOrderDetails,
    const std::map<int, Order> &orders,
    const std::map<int, PartType> &partTypes)
{
    std::map<int, std::vector<PartTypeOrderInfo>> finalInfoByMaterial;

    // 從每個 Material 類型中收集所有 OrderDetail
    for (const auto &materialEntry : sortedMaterialOrderDetails)
    {
        std::vector<OrderDetail> allOrderDetails = materialEntry.second;

        // 根據自定義比較器進行排序
        std::sort(allOrderDetails.begin(), allOrderDetails.end(),
                  [&](const OrderDetail &a, const OrderDetail &b)
                  {
                      return orderDetailComparator(a, b, orders, partTypes);
                  });

        // 遍歷排序後的 OrderDetail
        for (const auto &detail : allOrderDetails)
        {
            PartTypeOrderInfo info;
            info.PartType = detail.PartType; // 假設 OrderDetail 中有 PartTypeId
            info.Material = detail.Material; // 假設 OrderDetail 中有 MaterialId
            const Order &order = orders.at(detail.OrderId);

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
bool allMaterialsProcessed(const std::map<int, std::vector<PartTypeOrderInfo>> &sortedMaterials)
{
    for (const auto &materialEntry : sortedMaterials)
    {
        if (!materialEntry.second.empty())
        {

            return false;
        }
    }
    return true;
}

int selectMaterial(const std::map<int, std::vector<PartTypeOrderInfo>> &remainingMaterials)
{
    int selectedMaterial = -1;
    double earliestDueDate = std::numeric_limits<double>::max();
    double lowestPenaltyCost = std::numeric_limits<double>::max();
    double smallestVolume = std::numeric_limits<double>::max();

    for (const auto &materialEntry : remainingMaterials)
    {
        for (const auto &partInfo : materialEntry.second)
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
int selectMachineWithLeastRunningTime(const std::vector<MachineBatch> &machineBatches)
{
    int selectedMachineId = -1;
    double leastRunningTime = std::numeric_limits<double>::max();

    for (const auto &machineBatch : machineBatches)
    {
        if (machineBatch.RunningTime < leastRunningTime)
        {
            leastRunningTime = machineBatch.RunningTime;
            selectedMachineId = machineBatch.MachineId;
        }
    }

    return selectedMachineId;
}

double calculateFinishTime(const Batch &batch, const int &machineId, const std::vector<std::pair<int, Machine>> &sortedMachines)
{
    double volumeTime = 0.0;
    double maxHeight = 0.0;

    // 找到對應的機器
    const Machine *machine = nullptr;
    for (const auto &machinePair : sortedMachines)
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

    for (const auto &partInfo : batch.parts)
    {
        volumeTime += partInfo.PartType->Volume * machine->ScanTime;
        maxHeight = std::max(maxHeight, partInfo.PartType->Height);
    }

    double heightTime = maxHeight * machine->RecoatTime;

    return std::max(volumeTime, heightTime);
}
Batch allocateMaterialToMachine(MachineBatch &selectedMachineBatch, int selectedMaterial, std::map<int, std::vector<PartTypeOrderInfo>> &remainingMaterials)
{
    Batch newBatch;
    newBatch.materialType = selectedMaterial;
    newBatch.totalArea = 0.0;

    if (remainingMaterials.find(selectedMaterial) != remainingMaterials.end())
    {
        auto &materialList = remainingMaterials[selectedMaterial];
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

void updateRemainingMaterials(std::map<int, std::vector<PartTypeOrderInfo>> &remainingMaterials, int selectedMaterialType)
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

int selectEarliestMaterial(const std::map<int, std::vector<PartTypeOrderInfo>> &sortedMaterials)
{
    int selectedMaterial = -1;
    double earliestDueDate = std::numeric_limits<double>::max();
    double highestPenaltyCost = -std::numeric_limits<double>::max();
    double largestArea = -std::numeric_limits<double>::max();

    for (const auto &materialEntry : sortedMaterials)
    {
        if (!materialEntry.second.empty())
        {
            const auto &orderInfo = materialEntry.second.front().OrderInfo;
            const auto &partType = materialEntry.second.front().PartType;
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

void initializeMachines(std::vector<MachineBatch> &machineBatches, const std::vector<std::pair<int, Machine>> &sortedMachines, int selectedMaterial, const std::map<int, std::vector<PartTypeOrderInfo>> &sortedMaterials)
{
    for (const auto &machinePair : sortedMachines)
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

MachineBatch &findMachineBatchById(std::vector<MachineBatch> &machineBatches, int machineId)
{
    for (auto &machineBatch : machineBatches)
    {
        if (machineBatch.MachineId == machineId)
        {
            return machineBatch;
        }
    }

    throw std::runtime_error("MachineBatch with the specified ID not found.");
}
double calculateWeightedDelay(const Batch &batch, double runningTime)
{
    double weightedDelay = 0.0;

    for (const auto &partInfo : batch.parts)
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
    const std::map<int, std::vector<PartTypeOrderInfo>> &sortedMaterials,
    const std::vector<std::pair<int, Machine>> &sortedMachines)
{
    std::vector<MachineBatch> machineBatches;
    std::map<int, int> lastMaterialForMachine;

    // 初始化機器
    std::cout << "Initializing machines...\n";
    for (const auto &machinePair : sortedMachines)
    {
        MachineBatch machineBatch;
        machineBatch.MachineId = machinePair.first;
        machineBatch.MachineArea = machinePair.second.Area;
        machineBatch.RunningTime = 0.0;
        machineBatch.TotalWeightedDelay = 0.0;
        machineBatches.push_back(machineBatch);
        std::cout << "Initialized machine ID: " << machineBatch.MachineId << " with Area: " << machineBatch.MachineArea << "\n";
    }

    auto remainingMaterials = sortedMaterials;

    // 當還有材料需要處理時進行迴圈
    while (!remainingMaterials.empty())
    {
        // 選擇具有最早 DueDate 的材料
        int selectedMaterial = selectEarliestMaterial(remainingMaterials);

        // 選擇運行時間最短的機器
        int selectedMachineId = selectMachineWithLeastRunningTime(machineBatches);
        auto &selectedMachineBatch = findMachineBatchById(machineBatches, selectedMachineId);
        if (lastMaterialForMachine[selectedMachineId] != selectedMaterial)
        {

            const auto machineIt = std::find_if(sortedMachines.begin(), sortedMachines.end(),
                                                [selectedMachineId](const std::pair<int, Machine> &machinePair)
                                                {
                                                    return machinePair.first == selectedMachineId;
                                                });

            if (machineIt != sortedMachines.end())
            {
                double setupTimeSum = std::accumulate(machineIt->second.MaterialSetup[selectedMaterial].begin(),
                                                      machineIt->second.MaterialSetup[selectedMaterial].end(), 0.0);
                selectedMachineBatch.RunningTime += setupTimeSum;
            }
        }
        // 分配材料到機器
        Batch newBatch = allocateMaterialToMachine(selectedMachineBatch, selectedMaterial, remainingMaterials);
        double finishTime = calculateFinishTime(newBatch, selectedMachineBatch.MachineId, sortedMachines);
        selectedMachineBatch.RunningTime += finishTime;
        selectedMachineBatch.Batches.push_back(newBatch);
        // 計算延遲
        DelayedBatchInfo delayedInfo;
        delayedInfo.BatchIndex = selectedMachineBatch.Batches.size();
        delayedInfo.WeightedDelay = calculateWeightedDelay(newBatch, selectedMachineBatch.RunningTime);
        selectedMachineBatch.DelayedBatchInfo.push_back(delayedInfo);
        selectedMachineBatch.TotalWeightedDelay += delayedInfo.WeightedDelay;

        // 更新材料狀態
        updateRemainingMaterials(remainingMaterials, selectedMaterial);
        lastMaterialForMachine[selectedMachineId] = selectedMaterial;
    }

    return machineBatches;
}

void printMachineBatch(const std::vector<MachineBatch> MachineBatchs)
{

    for (const auto &machineBatch : MachineBatchs)
    {
        std::cout << "Machine ID: " << machineBatch.MachineId << "\n";
        std::cout << "Machine Area: " << machineBatch.MachineArea << "\n";
        std::cout << "Running Time: " << machineBatch.RunningTime << "\n";
        std::cout << "Total Weighted Delay: " << machineBatch.TotalWeightedDelay << "\n";
        std::cout << "Batches:\n";

        for (const auto &batch : machineBatch.Batches)
        {
            std::cout << "  Batch Material Type: " << batch.materialType << "\n";
            std::cout << "  Batch Total Area: " << batch.totalArea << "\n";
            std::cout << "  Parts:\n";
            for (const auto &part : batch.parts)
            {
                std::cout << "    Part Type ID: " << part.PartType->PartTypeId << "\n";
                std::cout << "    Order ID: " << part.OrderInfo.OrderId << "\n";
            }
        }

        std::cout << "Delayed Batches:\n";
        for (const auto &delayedBatch : machineBatch.DelayedBatchInfo)
        {
            std::cout << "  Batch Index: " << delayedBatch.BatchIndex << "\n";
            std::cout << "  Weighted Delay: " << delayedBatch.WeightedDelay << "\n";
        }
    }
}

double sumTotalWeightedDelay(const std::vector<MachineBatch> &machineBatches)
{
    double total = 0.0;
    for (const auto &batch : machineBatches)
    {
        total += batch.TotalWeightedDelay;
    }
    return total;
}

void read_json(const std::string &file_path)
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
    for (auto &kv : j["Machines"].items())
    {
        int key = std::stoi(kv.key());
        json value = kv.value();
        Machine m;
        m.MachineId = value["MachineId"];
        m.Area = value["Area"];
        m.Height = value["Height"];
        m.Length = value["Length"];
        m.Width = value["Width"];
        for (auto &material : value["Materials"])
        {
            m.Materials.push_back(material);
        }
        for (auto &setup : value["MaterialSetup"])
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
    for (auto &kv : j["PartTypes"].items())
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
    for (auto &kv : j["Orders"].items())
    {
        int key = std::stoi(kv.key());
        json value = kv.value();
        Order o;
        o.OrderId = value["OrderId"];
        o.DueDate = value["DueDate"];
        o.ReleaseDate = value["ReleaseDate"];
        o.PenaltyCost = value["PenaltyCost"];
        for (auto &detailValue : value["OrderList"])
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
    // printFinalSorted(finalSorted);

    auto machineBatches = createMachineBatches(finalSorted, sortedMachines);

    printMachineBatch(machineBatches);


    std::cout << "----------------------------------" << "\n";
    double result = sumTotalWeightedDelay(machineBatches);
    std::cout << "  初始解 : " << result << "\n";
}

int main()
{
    std::vector<std::string> files = {"test.json","tasks.json"};
    for (const auto &file : files)
    {
        read_json(file);
    }
    return 0;
}
