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

struct MachineBatch
{
    int MachineId;
    double MachineArea;
    std::vector<Batch> Batches;
};
struct DelayedBatchInfo {
    int machineId;       // 機器ID
    int batchIndex;      // 批次索引
    double penalty;      // 延遲懲罰
};
struct InitialSolutionResult {
    double firstResult;
    std::vector<double> maxPenaltyDelays;  // 每台機器的最大延遲懲罰
    std::vector<int> maxPenaltyBatchIndices;  // 每台機器的最大延遲懲罰對應的批次索引
    std::vector<DelayedBatchInfo> delayedBatches;  // 所有有延遲的批次
};
bool compareMachines(const std::pair<int, Machine> &a, const std::pair<int, Machine> &b)
{
    double totalTimeA = a.second.ScanTime + a.second.RecoatTime;
    double totalTimeB = b.second.ScanTime + b.second.RecoatTime;
    return totalTimeA > totalTimeB;
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
    return partA.Volume > partB.Volume;
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

        // 使用自定义比较器进行排序
        std::sort(sortedOrderDetails.begin(), sortedOrderDetails.end(),
                  [&](const OrderDetail &a, const OrderDetail &b)
                  {
                      return orderDetailComparator(a, b, orders, partTypes);
                  });

        sortedMaterialOrderDetails[entry.first] = sortedOrderDetails;
    }

    return sortedMaterialOrderDetails;
}

std::vector<PartTypeOrderInfo> generateFinalSortedPartTypes(
    const std::map<int, std::vector<OrderDetail>> &sortedMaterialOrderDetails,
    const std::map<int, Order> &orders,
    const std::map<int, PartType> &partTypes)
{
    std::vector<PartTypeOrderInfo> finalInfo;

    // 創建一個用於存儲所有 OrderDetail 的臨時向量
    std::vector<OrderDetail> allOrderDetails;

    // 從每個 Material 類型中收集所有 OrderDetail
    for (const auto &materialEntry : sortedMaterialOrderDetails)
    {
        allOrderDetails.insert(allOrderDetails.end(), materialEntry.second.begin(), materialEntry.second.end());
    }

    // 根據 DueDate 進行排序
    std::sort(allOrderDetails.begin(), allOrderDetails.end(),
              [&](const OrderDetail &a, const OrderDetail &b)
              {
                  const Order &orderA = orders.at(a.OrderId);
                  const Order &orderB = orders.at(b.OrderId);
                  return orderA.DueDate < orderB.DueDate;
              });

    // 添加排序後的 PartType ID 到最終向量
    for (const auto &detail : allOrderDetails)
    {
        PartTypeOrderInfo info;
        info.PartType = detail.PartType;
        info.Material = detail.Material; // 假設 OrderDetail 中有 MaterialId
        // info.Quantity = detail.Quantity;
        const Order &order = orders.at(detail.OrderId);

        // 创建 OrderInfo 并填充数据
        OrderInfo orderInfo;
        orderInfo.OrderId = order.OrderId;
        orderInfo.DueDate = order.DueDate;
        orderInfo.ReleaseDate = order.ReleaseDate;
        orderInfo.PenaltyCost = order.PenaltyCost;

        // 将 OrderInfo 赋值给 PartTypeOrderInfo 的 OrderInfo 成员
        info.OrderInfo = orderInfo;

        for (int i = 0; i < detail.Quantity; ++i)
        {
            finalInfo.push_back(info);
        }
    }

    return finalInfo;
}

std::vector<MachineBatch> createMachineBatches(
    const std::vector<PartTypeOrderInfo> &sortedMaterials,
    const std::vector<std::pair<int, Machine>> &sortedMachines)
{
    std::vector<MachineBatch> machineBatches;
    std::unordered_set<int> usedMaterialTypes;

    for (const auto &machinePair : sortedMachines)
    {
        MachineBatch machineBatch;
        machineBatch.MachineId = machinePair.first;
        machineBatch.MachineArea = machinePair.second.Area;

        double availableArea = machineBatch.MachineArea;
        std::vector<Batch> batches;
        Batch currentBatch;
        bool batchOpen = false;

        for (const auto &material : sortedMaterials)
        {
            if (usedMaterialTypes.find(material.Material) != usedMaterialTypes.end())
                continue;

            double partArea = material.PartType->Area;

            if (!batchOpen || (batchOpen && currentBatch.materialType == material.Material))
            {
                if (partArea <= availableArea)
                {
                    if (!batchOpen)
                    {
                        currentBatch = Batch();
                        currentBatch.materialType = material.Material;
                        currentBatch.totalArea = 0;
                        batchOpen = true;
                    }

                    currentBatch.parts.push_back(material);
                    currentBatch.totalArea += partArea;
                    availableArea -= partArea;
                }
                else
                {

                    if (batchOpen)
                    {
                        batches.push_back(currentBatch);
                        batchOpen = false;
                    }
                }
            }
            else
            {
                if (batchOpen)
                {
                    batches.push_back(currentBatch);
                    batchOpen = false;
                }
            }
        }
        if (batchOpen)
        {
            batches.push_back(currentBatch);
        }

        for (const auto &batch : batches)
        {
            usedMaterialTypes.insert(batch.materialType);
        }

        machineBatch.Batches = batches;
        machineBatches.push_back(machineBatch);
    }

    return machineBatches;
}

std::vector<double> calculateFinishTimes(const std::vector<MachineBatch> &machineBatches, std::map<int, Machine> machines)
{
    std::vector<double> finishTimes;

    for (const auto &machineBatch : machineBatches)
    {
        const Machine &machine = machines.at(machineBatch.MachineId);

        double volumeTime = 0.0;
        double maxHeight = 0.0;

        for (const auto &batch : machineBatch.Batches)
        {
            double batchVolume = 0.0;

            for (const auto &partInfo : batch.parts)
            {
                batchVolume += partInfo.PartType->Volume;
                maxHeight = std::max(maxHeight, partInfo.PartType->Height);
                // std::cout << "batchVolume: " << batchVolume << std::endl;
                // std::cout << "maxHeight: " << maxHeight << std::endl;
            }

            volumeTime += batchVolume * machine.ScanTime;

            // std::cout << "volumeTime: " << volumeTime << std::endl;
        }

        double heightTime = maxHeight * machine.RecoatTime;
        // std::cout << "heightTime: " << heightTime << std::endl;
        // std::cout << "volumeTime: " << volumeTime << std::endl;
        // std::cout << "--------------------- " << std::endl;

        double finishTime = std::max(volumeTime, heightTime);

        finishTimes.push_back(finishTime);
    }

    return finishTimes;
}
// 計算所有機台的總延遲懲罰費用並找出每台機器的最大延遲懲罰批次
InitialSolutionResult calculateTotalPenalty(const std::vector<MachineBatch> &machineBatches,
                                            const std::vector<double> &finishTimes) {
    InitialSolutionResult result;
    result.firstResult = 0.0;
    result.maxPenaltyDelays.resize(machineBatches.size(), std::numeric_limits<double>::lowest());
    result.maxPenaltyBatchIndices.resize(machineBatches.size(), -1);

    for (size_t i = 0; i < machineBatches.size(); ++i) {
        double machinePenalty = 0.0;

        for (size_t j = 0; j < machineBatches[i].Batches.size(); ++j) {
            const auto &batch = machineBatches[i].Batches[j];
            for (const auto &partInfo : batch.parts) {
                double deltaTime = partInfo.OrderInfo.DueDate - finishTimes[i];

                if (deltaTime < 0) {
                    double penaltyDelay = -deltaTime * partInfo.OrderInfo.PenaltyCost;
                    machinePenalty += penaltyDelay;

                    // 檢查並更新最大延遲懲罰
                    if (penaltyDelay > result.maxPenaltyDelays[i]) {
                        result.maxPenaltyDelays[i] = penaltyDelay;
                        result.maxPenaltyBatchIndices[i] = static_cast<int>(j);
                    }

                    // 添加到有延遲的批次列表中
                    DelayedBatchInfo delayedBatch;
                    delayedBatch.machineId = machineBatches[i].MachineId;
                    delayedBatch.batchIndex = static_cast<int>(j);
                    delayedBatch.penalty = penaltyDelay;
                    result.delayedBatches.push_back(delayedBatch);
                }
            }
        }

        result.firstResult += machinePenalty;
    }

    return result;
}


void printInitialSolutionResult(const InitialSolutionResult& result) {
    std::cout << "總延遲懲罰費用: " << result.firstResult << std::endl;

    for (size_t i = 0; i < result.maxPenaltyDelays.size(); ++i) {
        std::cout << "機器 " << i + 1 << " 的最大延遲懲罰: " << result.maxPenaltyDelays[i];
        std::cout << "，對應批次索引: " << result.maxPenaltyBatchIndices[i] << std::endl;
    }

    std::cout << "所有有延遲的批次：" << std::endl;
    for (const auto& delayedBatch : result.delayedBatches) {
        std::cout << "機器ID: " << delayedBatch.machineId;
        std::cout << ", 批次索引: " << delayedBatch.batchIndex;
        std::cout << ", 延遲懲罰: " << delayedBatch.penalty << std::endl;
    }
}


void printFinalSorted(const std::vector<PartTypeOrderInfo> &finalSorted)
{
    for (const auto &item : finalSorted)
    {
        std::cout << "Material: " << item.Material << ", "
                  //   << "Quantity: " << item.Quantity << ", "
                  << "PartType: " << item.PartType->PartTypeId << ", "
                  << "Part Area: " << item.PartType->Area << ", "
                  << "Order ID: " << item.OrderInfo.OrderId << ", "
                  << "Due Date: " << item.OrderInfo.DueDate << ", "
                  << "Release Date: " << item.OrderInfo.ReleaseDate << ", "
                  << "Penalty Cost: " << item.OrderInfo.PenaltyCost << std::endl;
    }
}

void printMachineBatches(const std::vector<MachineBatch> &machineBatches)
{
    for (const auto &machineBatch : machineBatches)
    {
        std::cout << "Machine ID: " << machineBatch.MachineId
                  << ", Area: " << machineBatch.MachineArea << std::endl;

        for (const auto &batch : machineBatch.Batches)
        {
            std::cout << "  Material Type: " << batch.materialType
                      << ", Total Area: " << batch.totalArea << std::endl;

            for (const auto &part : batch.parts)
            {
                std::cout << "    Part Type: " << part.PartType->PartTypeId
                          << ", Area: " << part.PartType->Area << std::endl;
            }
        }
        std::cout << std::endl;
    }
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

    std::vector<PartTypeOrderInfo> finalSorted = generateFinalSortedPartTypes(sortedMaterials, orders, partTypes); // 这里应填入正确的参数
    // printFinalSorted(finalSorted);

    auto machineBatches = createMachineBatches(finalSorted, sortedMachines);

    // printMachineBatches(machineBatches);

    auto finishTime = calculateFinishTimes(machineBatches, machines);

    auto firstResult = calculateTotalPenalty(machineBatches, finishTime);
    printInitialSolutionResult(firstResult);

    
}

int main()
{
    std::vector<std::string> files = {"test.json"};
    for (const auto &file : files)
    {
        read_json(file);
    }
    return 0;
}
