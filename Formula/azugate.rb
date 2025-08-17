class Azugate < Formula
  desc "High-performance reverse proxy and load balancer with file serving capabilities"
  homepage "https://github.com/Azusain/azugate"
  url "https://github.com/Azusain/azugate/archive/refs/tags/v1.1.1.tar.gz"
  sha256 "" # This will be filled when creating a real release
  license "MIT"
  head "https://github.com/Azusain/azugate.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "boost"
  depends_on "yaml-cpp"
  depends_on "nlohmann-json"
  depends_on "spdlog"
  depends_on "fmt"
  depends_on "cxxopts"
  depends_on "openssl@3"

  def install
    # Create a temporary vcpkg installation for JWT-cpp and GTest
    system "git", "clone", "https://github.com/Microsoft/vcpkg.git", "vcpkg"
    system "./vcpkg/bootstrap-vcpkg.sh"
    system "./vcpkg/vcpkg", "install", "jwt-cpp", "gtest"

    # Configure with CMake
    args = %W[
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_TOOLCHAIN_FILE=#{buildpath}/vcpkg/scripts/buildsystems/vcpkg.cmake
      -DOPENSSL_ROOT_DIR=#{Formula["openssl@3"].opt_prefix}
      -DCMAKE_INSTALL_PREFIX=#{prefix}
    ]

    system "cmake", "-B", "build", "-G", "Ninja", *args
    system "cmake", "--build", "build", "--config", "Release"
    system "cmake", "--install", "build"
  end

  def post_install
    # Create config directory
    (etc/"azugate").mkpath
    
    # Copy default config if it doesn't exist
    unless (etc/"azugate/azugate.yaml").exist?
      cp pkgshare/"doc/azugate/config.default.yaml", etc/"azugate/azugate.yaml"
    end
  end

  service do
    run [opt_bin/"azugate", "-c", etc/"azugate/azugate.yaml"]
    keep_alive true
    log_path var/"log/azugate.log"
    error_log_path var/"log/azugate.log"
  end

  test do
    # Test that the binary runs and shows help
    system "#{bin}/azugate", "--help"
    
    # Test config generation
    system "#{bin}/azugate", "--generate-config", testpath/"test.yaml"
    assert_predicate testpath/"test.yaml", :exist?
    
    # Test config validation
    system "#{bin}/azugate", "--validate-config", testpath/"test.yaml"
  end
end
