"use client";
import { useEffect, useState } from "react";

interface FirewallTableInterface {
  ipList: string[];
}

const FirewallTable = () => {
  const [ipList, setipList] = useState<string[]>();
  useEffect(() => {
    const getIpList = async () => {
      const response = await fetch("http://localhost:8081/config/iplist", {
        method: "GET",
        headers: {
          "Content-Type": "application/json",
        },
      });
      const result: FirewallTableInterface = await response.json();
      setipList(result.ipList);
    };
    getIpList();
  }, []);

  return (
    <div className="overflow-x-auto">
      <table className="table">
        {/* head */}
        <thead>
          <tr>
            <th></th>
            <th>IP</th>
          </tr>
        </thead>
        {ipList ? (
          <tbody>
            {ipList.map((ip, idx) => {
              return (
                <tr key={idx}>
                  <th>{idx}</th>
                  <td>{ip}</td>
                </tr>
              );
            })}
          </tbody>
        ) : (
          <></>
        )}
      </table>
    </div>
  );
};

export default function Home() {
  const [sidebar, setSidebar] = useState("Firewall");
  return (
    <div className="flex flex-row">
      {/* sidebar */}
      <div className="drawer lg:drawer-open w-1/4">
        <input id="my-drawer-2" type="checkbox" className="drawer-toggle" />
        <div className="drawer-content flex flex-col items-center justify-center">
          {/* Page content here */}
          <label
            htmlFor="my-drawer-2"
            className="btn btn-primary drawer-button lg:hidden"
          >
            Open drawer
          </label>
        </div>
        <div className="drawer-side">
          <label
            htmlFor="my-drawer-2"
            aria-label="close sidebar"
            className="drawer-overlay"
          ></label>
          <ul className="menu bg-base-200 text-base-content min-h-full w-80 p-4">
            {/* Sidebar content here */}
            <div className="font-semibold text-lg p-4">azugate</div>
            <div className="divider"></div>
            <li>
              <a
                className={
                  sidebar == "Overview" ? "bg-gray-700 text-white" : ""
                }
                onClick={() => {
                  setSidebar("Overview");
                }}
              >
                Overview
              </a>
            </li>
            <li>
              <a
                className={
                  sidebar == "Firewall" ? "bg-gray-700 text-white" : ""
                }
                onClick={() => {
                  setSidebar("Firewall");
                }}
              >
                Firewall
              </a>
            </li>
          </ul>
        </div>
      </div>
      {/* navigation */}
      <div className="w-3/4">
        <div className="navbar bg-base-100">
          <div className="navbar-start"></div>
          <div className="navbar-center"></div>
          <div className="navbar-end">
            <button className="btn btn-ghost btn-circle">
              <svg
                xmlns="http://www.w3.org/2000/svg"
                className="h-5 w-5"
                fill="none"
                viewBox="0 0 24 24"
                stroke="currentColor"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth="2"
                  d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z"
                />
              </svg>
            </button>
            <button className="btn btn-ghost btn-circle">
              <div className="indicator">
                <svg
                  xmlns="http://www.w3.org/2000/svg"
                  className="h-5 w-5"
                  fill="none"
                  viewBox="0 0 24 24"
                  stroke="currentColor"
                >
                  <path
                    strokeLinecap="round"
                    strokeLinejoin="round"
                    strokeWidth="2"
                    d="M15 17h5l-1.405-1.405A2.032 2.032 0 0118 14.158V11a6.002 6.002 0 00-4-5.659V5a2 2 0 10-4 0v.341C7.67 6.165 6 8.388 6 11v3.159c0 .538-.214 1.055-.595 1.436L4 17h5m6 0v1a3 3 0 11-6 0v-1m6 0H9"
                  />
                </svg>
                <span className="badge badge-xs badge-primary indicator-item"></span>
              </div>
            </button>
          </div>
        </div>
        {sidebar == "Firewall" ? <FirewallTable /> : <></>}
      </div>
    </div>
  );
}
