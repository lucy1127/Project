#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>

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

struct Machine
{
    int MachineId;
    double Area, Height, Length, Width;
    std::vector<int> Materials;
    std::vector<std::vector<double>> MaterialSetup;
    std::vector<double> StartSetup;
    double ScanTime, RecoatTime, RemovalTime;
};

struct Batch
{
    int materialType;
    std::vector<PartType> parts;
    double totalArea;
};

struct MachineBatch
{
    int MachineId;
    double MachineArea;
    std::vector<Batch> Batches;
};

void printMachine(const Machine &machine)
{
    std::cout << "Machine ID: " << machine.MachineId << ", Area: " << machine.Area
              << ", Height: " << machine.Height << ", Length: " << machine.Length
              << ", Width: " << machine.Width << std::endl;
    std::cout << "\tMaterials: ";
    for (const auto &material : machine.Materials)
    {
        std::cout << material << " ";
    }
    std::cout << "\n\tMaterial Setup Times: ";
    for (const auto &setupTimes : machine.MaterialSetup)
    {
        for (auto time : setupTimes)
        {
            std::cout << time << " ";
        }
        std::cout << "; ";
    }
    std::cout << "\n\tStart Setup Times: ";
    for (const auto &time : machine.StartSetup)
    {
        std::cout << time << " ";
    }
    std::cout << "\n\tScan Time: " << machine.ScanTime
              << "\n\tRecoat Time: " << machine.RecoatTime
              << "\n\tRemoval Time: " << machine.RemovalTime << std::endl;
}
void printPartType(const PartType &part)
{
    std::cout << "PartType ID: " << part.PartTypeId << ", Height: " << part.Height
              << ", Length: " << part.Length << ", Width: " << part.Width
              << ", Area: " << part.Area << ", Volume: " << part.Volume << std::endl;
}
void printOrderDetail(const OrderDetail &detail, const std::map<int, PartType> &partTypes, const std::map<int, Order> &orders)
{
    const PartType &part = partTypes.at(detail.PartType->PartTypeId);
    std::cout << "\tPartType ID: " << part.PartTypeId
              << ", Quantity: " << detail.Quantity
              << ", Material: " << detail.Material
              << ", Quality: " << detail.Quality << std::endl;
    std::cout << "\t\tPartType Details: Height: " << part.Height
              << ", Length: " << part.Length
              << ", Width: " << part.Width
              << ", Area: " << part.Area
              << ", Volume: " << part.Volume << std::endl;

    // 遍歷 orders 映射，尋找包含當前 PartType 的訂單
    for (const auto &orderPair : orders)
    {
        const Order &order = orderPair.second;
        for (const auto &od : order.OrderList)
        {
            if (od.PartType->PartTypeId == detail.PartType->PartTypeId)
            {
                // 當找到包含當前 PartType 的訂單時，打印訂單信息
                std::cout << "\t\t\tFrom Order ID: " << order.OrderId
                          << ", DueDate: " << order.DueDate
                          << ", ReleaseDate: " << order.ReleaseDate
                          << ", PenaltyCost: " << order.PenaltyCost << std::endl;
                // 可能只想打印一次訂單信息，如果是這樣，打印後可以跳出循環
                break;
            }
        }
    }
}
// 在呼叫 printOrderDetail 時傳遞 orders 映射
void printOrder(const Order &order, const std::map<int, PartType> &partTypes, const std::map<int, Order> &orders)
{
    std::cout << "Order ID: " << order.OrderId
              << ", DueDate: " << order.DueDate
              << ", ReleaseDate: " << order.ReleaseDate
              << ", PenaltyCost: " << order.PenaltyCost << std::endl;
    for (const auto &detail : order.OrderList)
    {
        printOrderDetail(detail, partTypes, orders); // 傳遞 partTypes 和 orders 參數
    }
}

bool compareMachines(const std::pair<int, Machine> &a, const std::pair<int, Machine> &b)
{
    double totalTimeA = a.second.ScanTime + a.second.RecoatTime;
    double totalTimeB = b.second.ScanTime + b.second.RecoatTime;
    return totalTimeA > totalTimeB; // Change to < for ascending order
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

std::vector<int> generateFinalSortedPartTypes(
    const std::map<int, std::vector<OrderDetail>> &sortedMaterialOrderDetails,
    const std::map<int, Order> &orders,
    const std::map<int, PartType> &partTypes)
{
    std::vector<int> finalSortedPartTypes;

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
        for (int i = 0; i < detail.Quantity; ++i)
        {
            finalSortedPartTypes.push_back(detail.PartType->PartTypeId);
        }
    }

    return finalSortedPartTypes;
}

void printFinalSortedPartTypes(const std::vector<int> &finalSortedPartTypes)
{
    for (int partTypeId : finalSortedPartTypes)
    {
        std::cout << partTypeId << ' ';
    }
    std::cout << std::endl; // 在输出完毕后换行
}
void printSortedMaterialOrderDetails(const std::map<int, std::vector<OrderDetail>> &sortedMaterialOrderDetails)
{
    for (const auto &materialEntry : sortedMaterialOrderDetails)
    {
        int materialType = materialEntry.first;
        const auto &orderDetails = materialEntry.second;

        std::cout << "Material Type: " << materialType << std::endl;

        for (const auto &detail : orderDetails)
        {
            std::cout << "  Order ID: " << detail.OrderId
                      << ", Quantity: " << detail.Quantity
                      << ", Material: " << detail.Material
                      << ", Quality: " << detail.Quality;

            if (detail.PartType != nullptr)
            {
                std::cout << ", Part Type ID: " << detail.PartType->PartTypeId;
                // 这里可以继续打印 PartType 的其他字段
            }

            std::cout << std::endl;
        }

        std::cout << std::endl;
    }
}

std::vector<MachineBatch> createMachineBatches(
    const std::map<int, std::vector<OrderDetail>> &sortedMaterials,
    const std::vector<std::pair<int, Machine>> &sortedMachines,
    const std::map<int, Order> &orders)
{
    std::vector<MachineBatch> machineBatches;
    std::map<int, int> materialQuantities; // 存储每种材料的剩余数量

    // 初始化材料数量
    for (const auto &material : sortedMaterials)
    {
        for (const auto &detail : material.second)
        {
            materialQuantities[material.first] += detail.Quantity;
        }
    }

    // 根据 DueDate 对材料类型进行排序
    std::vector<int> materialTypes;
    for (const auto &entry : sortedMaterials)
    {
        materialTypes.push_back(entry.first);
    }
    std::sort(materialTypes.begin(), materialTypes.end(),
              [&](int lhs, int rhs)
              {
                  double minDueDateLhs = std::numeric_limits<double>::max();
                  double minDueDateRhs = std::numeric_limits<double>::max();
                  for (const auto &detail : sortedMaterials.at(lhs))
                  {
                      minDueDateLhs = std::min(minDueDateLhs, orders.at(detail.OrderId).DueDate);
                  }
                  for (const auto &detail : sortedMaterials.at(rhs))
                  {
                      minDueDateRhs = std::min(minDueDateRhs, orders.at(detail.OrderId).DueDate);
                  }
                  return minDueDateLhs < minDueDateRhs;
              });

    for (const auto &machinePair : sortedMachines)
    {
        MachineBatch machineBatch;
        machineBatch.MachineId = machinePair.first;
        machineBatch.MachineArea = machinePair.second.Area;

        double availableArea = machinePair.second.Area;
        std::vector<Batch> batches;

        for (int materialType : materialTypes)
        {
            double totalArea = 0.0;
            std::vector<PartType> partsInBatch;

            for (auto &orderDetail : sortedMaterials.at(materialType))
            {
                // 只处理尚有剩余数量的材料
                if (materialQuantities[materialType] > 0)
                {
                    double partArea = orderDetail.PartType->Area * orderDetail.Quantity;
                    if (totalArea + partArea <= availableArea && materialQuantities[materialType] >= orderDetail.Quantity)
                    {
                        partsInBatch.push_back(*orderDetail.PartType);
                        totalArea += partArea;
                        materialQuantities[materialType] -= orderDetail.Quantity; // 减少剩余数量
                    }
                    else
                    {
                        break; // 如果无法放置当前部件，则停止尝试放置该材料类型的更多部件
                    }
                }
            }

            if (!partsInBatch.empty())
            {
                batches.push_back({materialType, partsInBatch, totalArea});
                availableArea -= totalArea;
            }
        }

        machineBatch.Batches = batches;
        machineBatches.push_back(machineBatch);
    }

    return machineBatches;
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
        // printMachine(m); // Print Machine information
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
        // printPartType(p);  // 打印 PartType 信息
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
            // printOrderDetail(od, partTypes, orders);
        }
        orders[o.OrderId] = o;
        // printOrder(o, partTypes, orders);
    }

    // 打印按 Material 分類的訂單細節
    // Printing material classified order details
    for (const auto &entry : materialClassifiedOrderDetails)
    {
        // std::cout << "Material: " << entry.first << std::endl;
        for (const auto &detail : entry.second)
        {
            // Correctly passing all required arguments to the printOrderDetail function
            // printOrderDetail(detail, partTypes, orders); // Pass the maps partTypes and orders here
        }
    }

    auto sortedMaterials = sortMaterialClassifiedOrderDetails(materialClassifiedOrderDetails, orders, partTypes);

    // printSortedMaterialOrderDetails(sortedMaterials);
    // 假设这里是从之前函数中获取到的finalSortedPartTypes
    std::vector<int> finalSortedPartTypes = generateFinalSortedPartTypes(sortedMaterials,orders,partTypes); // 这里应填入正确的参数

    // 打印最终排序的零件类型ID
    printFinalSortedPartTypes(finalSortedPartTypes);

    auto machineBatches = createMachineBatches(sortedMaterials, sortedMachines, orders);

    // 打印机器和分配的批次信息
    for (const auto &machineBatch : machineBatches)
    {
        // std::cout << "Machine ID: " << machineBatch.MachineId << ", Area: " << machineBatch.MachineArea << std::endl;
        // for (const auto &batch : machineBatch.Batches)
        // {
        //     std::cout << "\tMaterial Type: " << batch.materialType << ", Total Area: " << batch.totalArea << std::endl;
        //     for (const auto &part : batch.parts)
        //     {
        //         std::cout << "\t\tPart Type: " << part.PartTypeId << ", Area: " << part.Area << std::endl;
        //     }
        // }
    }
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
