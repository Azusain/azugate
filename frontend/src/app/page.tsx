"use client";
import React, { useState } from "react";
import {
  Header,
  Sidebar,
  FirewallMainContent,
  SettingsMainContent,
} from "./components";

export default function Home() {
  const [sidebar, setSidebar] = useState("Firewall");

  return (
    <div className="flex flex-row">
      <div className="w-1/4">
        <Sidebar setSidebar={setSidebar} sidebar={sidebar} />
      </div>
      <div className="w-3/4">
        <Header />
        {/* main content */}
        {sidebar == "Firewall" && <FirewallMainContent />}
        {sidebar == "Settings" && <SettingsMainContent />}
      </div>
    </div>
  );
}

// TODO:
// static resources should be download beforehand.
// create a folder for api codes.
