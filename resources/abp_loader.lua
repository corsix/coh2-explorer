local function normalise(path)
  if not path:find("..", 1, true) then
    return path
  end
  
  local index = 0
  local parts = {}
  local discard = {}
  for part in path:gmatch("[^/\\]+") do
    index = index + 1
    local N = #parts
    if part == ".." then
      discard[parts[N]] = true
      discard[index] = true
      parts[N] = nil
    else
      parts[N + 1] = index
    end
  end
  index = 0
  return (path:gsub("[^/\\]+.?", function(s)
    index = index + 1
    if discard[index] then
      return ""
    else
      return s
    end
  end))
end

local seen = {}
local filelist = {}

local loadable_extensions = {
  rga = true,
  rgm = true,
  rgo = true,
  rpb = true,
}

local function import(path)
  if seen[path] then
    return
  end
  seen[path] = true
  
  local current_dir = path:gsub("[^/\\]*$", "")
  if path:sub(-4) == ".abp" then
    local env = {}
    loadfile(path, "t", env)()
    env = env.model
    if env then
      for i = 1, #env do
        import(normalise(current_dir .. env[i]:lower()))
      end
    end
  elseif path:sub(-4, -3) == ".r" then
    filelist[#filelist + 1] = path
  else
    local has_any
    local files = files_matching_prefix(current_dir, path:sub(1 + #current_dir):lower() ..".")
    for i = 1, #files do
      local file = files[i]
      local ext = file:sub(-3)
      if loadable_extensions[ext] then
        import(current_dir .. file)
        has_any = true
      end
    end
    if not has_any then
      import(path ..".abp")
    end
  end
end

return import, filelist
