"use client";
import Image from "next/image";
import Link from "next/link";

export default function Login() {
  const AvatarInput = ({
    type,
    placeholder,
    avatarUrl,
  }: {
    type: string;
    placeholder: string;
    avatarUrl: string;
  }) => {
    return (
      <div className="relative mb-4">
        {/* Avatar */}
        <div className="absolute inset-y-0 left-0 flex items-center pl-3">
          <Image
            src={avatarUrl}
            alt="avatar"
            width={16}
            height={16}
            className="h-8 w-8 rounded-full"
          />
        </div>
        {/* Input */}
        <input
          type={type}
          placeholder={placeholder}
          className="input input-bordered w-full pl-14"
        />
      </div>
    );
  };

  return (
    <div className="flex h-screen items-center justify-center bg-gray-100">
      <div className="w-full max-w-sm rounded-lg bg-white p-8 shadow-lg">
        <h1 className="mb-4 text-center text-2xl font-bold text-gray-800">
          Login
        </h1>
        <form>
          {/* Username with avatar */}
          <AvatarInput
            type="text"
            placeholder="Username"
            avatarUrl="icons/user.svg"
          />
          {/* Password with avatar */}
          <AvatarInput
            type="password"
            placeholder="Password"
            avatarUrl="icons/lock.svg"
          />
          {/* Login button */}
          <Link href={"/"}>
            {" "}
            <button className="btn btn-primary w-full">Log In</button>
          </Link>{" "}
        </form>
      </div>
    </div>
  );
}
