#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct PartType {
    int PartTypeId;
    double Height;
    double Length;
    double Width;
    double Area;
    double Volume;
};

struct OrderDetail {
    PartType* PartType;  
    int Quantity;
    int Material;
    int Quality;
};

struct Order {
    int OrderId;
    double DueDate;
    double ReleaseDate;
    double PenaltyCost;
    std::vector<OrderDetail> OrderList;
};

struct Machine {
    int MachineId;
    double Area, Height, Length, Width;
    std::vector<int> Materials;
};

// Function to print machine details
void printMachine(const Machine& machine) {
    std::cout << "Machine ID: " << machine.MachineId << ", Area: " << machine.Area
              << ", Height: " << machine.Height << ", Length: " << machine.Length
              << ", Width: " << machine.Width << std::endl;
    std::cout << "\tMaterials: ";
    for (const auto& material : machine.Materials) {
        std::cout << material << " ";
    }
    std::cout << std::endl;
}
void printPartType(const PartType& part) {
    std::cout << "PartType ID: " << part.PartTypeId << ", Height: " << part.Height
              << ", Length: " << part.Length << ", Width: " << part.Width
              << ", Area: " << part.Area << ", Volume: " << part.Volume << std::endl;
}
void printOrderDetail(const OrderDetail& detail) {
    std::cout << "\tPartType ID: " << detail.PartType->PartTypeId 
              << ", Quantity: " << detail.Quantity 
              << ", Material: " << detail.Material 
              << ", Quality: " << detail.Quality << std::endl;
    std::cout << "\t\tPartType Details: Height: " << detail.PartType->Height 
              << ", Length: " << detail.PartType->Length 
              << ", Width: " << detail.PartType->Width 
              << ", Area: " << detail.PartType->Area 
              << ", Volume: " << detail.PartType->Volume << std::endl;

  // 打印該 PartType 對應的所有訂單信息
    for (const auto& orderPair : orders) {
        const Order& order = orderPair.second;
        for (const auto& od : order.OrderList) {
            if (od.PartType == detail.PartType) {
                std::cout << "\t\ttOrder ID: " << order.OrderId 
                          << ", DueDate: " << order.DueDate 
                          << ", ReleaseDate: " << order.ReleaseDate 
                          << ", PenaltyCost: " << order.PenaltyCost << std::endl;
            }
        }
    }
}

// 在呼叫 printOrderDetail 時傳遞 orders 映射
void printOrder(const Order& order, const std::map<int, Order>& orders) {
    std::cout << "Order ID: " << order.OrderId << ", DueDate: " << order.DueDate
              << ", ReleaseDate: " << order.ReleaseDate << ", PenaltyCost: " << order.PenaltyCost << std::endl;
       for (const auto& detail : order.OrderList) {
        printOrderDetail(detail, orders);  // 現在傳遞 orders 參數
    }
}

void read_json(const std::string &file_path) {
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


//   // Output additional parameters for verification
//     std::cout << "Instance ID: " << instanceID 
//               << "\\nSeed Value: " << seedValue 
//               << "\\nNumber of Orders: " << nOrders 
//               << "\\nItems per Order: " << nItemsPerOrder 
//               << "\\nTotal Items: " << nTotalItems 
//               << "\\nNumber of Machines: " << nMachines 
//               << "\\nNumber of Materials: " << nMaterials 
//               << "\\nPercentage of Tardy Orders: " << PercentageOfTardyOrders 
//               << "\\nDue Date Range: " << DueDateRange << std::endl;


    // 解析 Machines 部分
    std::map<int, Machine> machines;
    for (auto &kv : j["Machines"].items()) {
        int key = std::stoi(kv.key());
        json value = kv.value();
        Machine m;
        m.MachineId = value["MachineId"];
        m.Area = value["Area"];
        m.Height = value["Height"];
        m.Length = value["Length"];
        m.Width = value["Width"];
        for (auto &material : value["Materials"]) {
            m.Materials.push_back(material);
        }
        machines[key] = m;
        // printMachine(m);  // 打印 Machine 信息
    }

    // Output data for verification
    // std::cout << "Instance ID: " << instanceID << std::endl;


 // 解析 PartTypes 部分
    std::map<int, PartType> partTypes;
    for (auto &kv : j["PartTypes"].items()) {
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
    for (auto &kv : j["Orders"].items()) {
        int key = std::stoi(kv.key());
        json value = kv.value();
        Order o;
        o.OrderId = value["OrderId"];
        o.DueDate = value["DueDate"];
        o.ReleaseDate = value["ReleaseDate"];
        o.PenaltyCost = value["PenaltyCost"];
        for (auto &detail : value["OrderList"]) {
            OrderDetail od;
            int partTypeId = detail["PartType"];
            od.PartType = &partTypes[partTypeId];  // 引用 PartType 結構
            od.Quantity = detail["Quantity"];
            od.Material = detail["Material"];
            od.Quality = detail["Quality"];
            materialClassifiedOrderDetails[od.Material].push_back(od);
            o.OrderList.push_back(od);
        }
        orders[key] = o;
        printOrder(o);  // 打印 Order 信息
    }

    // 打印按 Material 分類的訂單細節
    for (const auto &entry : materialClassifiedOrderDetails) {
        std::cout << "Material: " << entry.first << std::endl;
        for (const auto &detail : entry.second) {
            printOrderDetail(detail);
        }
    }


}



int main() {
    std::vector<std::string> files = {"file.json"}; // Replace with your file name
    for (const auto &file : files) {
        read_json(file);
    }
    return 0;
}
